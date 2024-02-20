#include <riscv.h>
#include <iostream>
#include <str.h>
#include <random>
#include <cassert>
#include <algorithm>

const std::string reg_names[REG_NUM] = {
    "zero", "ra", "sp", "gp", "tp",
    "t0", "t1", "t2",
    "fp", "s1",
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
    "t3", "t4", "t5", "t6"};

const int free_regs[FREE_REG_NUM] = {
    5, 6, 7,                        // t0-2
    10, 11, 12, 13, 14, 15, 16, 17, // a0-7
    28, 29                          // t3-4
};

const int saved_regs[SAVED_REG_NUM] = {
    8, 9,                                   // fp s1
    18, 19, 20, 21, 22, 23, 24, 25, 26, 27, // s2-11
};

int regname_to_idx(const std::string name)
{
    for (int i = 0; i < REG_NUM; i++)
        if (reg_names[i] == name)
            return i;
    assert(0);
}

void safe_mem(const std::string op, const std::string reg_name, const int loc, RISCV &riscv, const std::string base)
{
    if (loc <= IMM12_MAX && loc > -IMM12_MAX)
        riscv.text.push_back({op, reg_name, std::to_string(-loc) + "(" + base + ")"});
    else
    {
        riscv.text.push_back({"li", "t5", std::to_string(loc)});
        riscv.text.push_back({"sub", "t5", base, "t5"});
        riscv.text.push_back({op, reg_name, "0(t5)"});
    }
}

void Controller::var_mem(const std::string op, const std::string name, const std::string reg_name, RISCV &riscv)
{
    if (glob->global_var.count(name))
    {
        riscv.text.push_back({"la", "t5", glob->global_var.at(name)});
        riscv.text.push_back({op, reg_name, "0(t5)"});
    }
    else
        safe_mem(op, reg_name, func->get_save_pos(name), riscv);
}

std::string transform_arg_name(const std::string name)
{
    if (start_with(name, "%"))
        return "@" + name.substr(5, name.find(":") - 5);
    return name.substr(0, name.find(":"));
}

void Controller::clear(const std::vector<std::string> &args)
{
    ptr.clear();
    int argc = args.size();
    func->save_pos.clear();
    reg_pos.clear();
    current_save.clear();
    for (int i = 1; i < SAVED_REG_NUM; i++)
        current_save.insert({"saved " + std::to_string(i), saved_regs[i]});
    label_set.clear();
    func->mem_need = 4 * SAVED_REG_NUM;
    reg_in_use[0] = "zero";
    reg_in_use[1] = "return address";
    reg_in_use[2] = "stack pointer";
    reg_in_use[3] = "global pointer";
    reg_in_use[4] = "thread pointer";
    for (int i = 0; i < 3; i++)
        reg_in_use[i + 5] = std::nullopt;
    reg_in_use[8] = "frame pointer";
    reg_in_use[9] = "saved 1";
    for (int i = 0; i < std::min(8, argc); i++)
    {
        bind(reg_names[i + 10], transform_arg_name(args[i]));
        func->save_pos[transform_arg_name(args[i])] = (1 + i - argc) * 4;
    }
    for (int i = 8; i < argc; i++)
        func->save_pos[transform_arg_name(args[i])] = -(i - 8) * 4;
    for (int i = argc; i < 8; i++)
        reg_in_use[i + 10] = std::nullopt;
    for (int i = 2; i < 12; i++)
        reg_in_use[i + 16] = "saved " + std::to_string(i);
    for (int i = 3; i < 7; i++)
        reg_in_use[i + 25] = std::nullopt;
    current_time = 0;
    for (int i = 0; i < REG_NUM; i++)
        last_used[i] = 0;
}

void Controller::refresh(RISCV &riscv, bool save, std::vector<std::string> except)
{
    for (int i = 0; i < FREE_REG_NUM; i++)
    {
        int idx = free_regs[i];
        if (reg_in_use[idx].has_value())
        {
            if (!is_allocvar(reg_in_use[idx].value()) && std::find(except.begin(), except.end(), reg_in_use[idx].value()) != except.end())
                continue;
            if (save || glob->global_var.count(reg_in_use[idx].value()))
                var_mem("sw", reg_in_use[idx].value(), reg_names[idx], riscv);
            reg_pos[reg_in_use[idx].value()] = std::nullopt;
            reg_in_use[idx] = std::nullopt;
        }
    }
    current_time = 0;
    for (int i = 0; i < REG_NUM; i++)
        last_used[i] = 0;
}

void Controller::transition(RISCV &riscv, std::string mode)
{
    for (auto &pair : current_save)
        if (glob->global_var.count(pair.first))
            var_mem(mode, pair.first, reg_names[pair.second], riscv);
}

void Controller::prepare_return(RISCV &riscv)
{
    for (auto const &i : current_save)
    {
        if (glob->global_var.count(i.first))
            var_mem("sw", i.first, reg_names[i.second], riscv);
        for (int j = 1; j < SAVED_REG_NUM; j++)
            if (i.second == saved_regs[j] && !start_with(i.first, "saved "))
                riscv.text.push_back({"lw", reg_names[i.second], std::to_string(-(j + 1) * 4) + "(fp)"});
    }
    return;
}

void Controller::bind(const std::string reg, const std::string name)
{
    reg_in_use[regname_to_idx(reg)] = name;
    reg_pos[name] = regname_to_idx(reg);
    last_used[regname_to_idx(reg)] = current_time++;
}

void Controller::alloc(const std::string name, RISCV &riscv, bool reg, int size)
{
    if (func->save_pos.count(name))
        return;
    if (size != 4)
        reg = false;
    if (func)
    {
        func->mem_need += size;
        func->save_pos[name] = func->mem_need;
    }
    if (reg)
        for (int i = 0; i < FREE_REG_NUM; i++)
        {
            int idx = free_regs[i];
            if (!reg_in_use[idx].has_value())
            {
                reg_in_use[idx] = name;
                reg_pos[name] = idx;
                last_used[idx] = current_time++;
                return;
            }
        }
    reg_pos[name] = std::nullopt;
}

int Controller::find_lru()
{
    int min_time = (1<<30);
    int min_idx = 0;
    for (int i = 0; i < FREE_REG_NUM; i++)
    {
        int idx = free_regs[i];
        if (last_used[idx] < min_time)
        {
            min_time = last_used[idx];
            min_idx = idx;
        }
    }
    return min_idx;
}

void Controller::save_back(int reg, RISCV &riscv, bool sync)
{
    if (reg_in_use[reg].has_value())
    {
        var_mem("sw", reg_in_use[reg].value(), reg_names[reg], riscv);
        if (sync)
        {
            reg_pos[reg_in_use[reg].value()] = std::nullopt;
            reg_in_use[reg] = std::nullopt;
        }
    }
}

int Controller::find_reg(RISCV &riscv)
{
    for (int i = 0; i < FREE_REG_NUM; i++)
    {
        int idx = free_regs[i];
        if (!reg_in_use[idx].has_value())
        {
            last_used[idx] = current_time++;
            return idx;
        }
    }
    int reg = find_lru();
    save_back(reg, riscv);
    last_used[reg] = current_time++;
    reg_pos[reg_in_use[reg].value()] = std::nullopt;
    reg_in_use[reg] = std::nullopt;
    return reg;
}

int Controller::load(const std::string name, RISCV &riscv, bool load, int specify)
{
    if (specify)
    {
        if (reg_pos[name].has_value() && reg_pos[name].value() == specify)
        {
            last_used[reg_pos[name].value()] = current_time++;
            return reg_pos[name].value();
        }
        if (reg_in_use[specify].has_value())
        {
            var_mem("sw", reg_in_use[specify].value(), reg_names[specify], riscv);
            reg_pos[reg_in_use[specify].value()] = std::nullopt;
            reg_in_use[specify] = std::nullopt;
        }
        if (reg_pos[name].has_value())
        {
            riscv.text.push_back({"mv", reg_names[specify], reg_names[reg_pos[name].value()]});
            reg_in_use[reg_pos[name].value()] = std::nullopt;
        }
        else
            var_mem("lw", name, reg_names[specify], riscv);
        last_used[specify] = current_time++;
        reg_in_use[specify] = name;
        reg_pos[name] = specify;
    }

    if (name == "!t6")
        return T6_REG;
    if (name == "!zero")
        return ZERO_REG;

    if (reg_pos[name].has_value())
    {
        last_used[reg_pos[name].value()] = current_time++;
        return reg_pos[name].value();
    }

    int reg = find_reg(riscv);
    reg_in_use[reg] = name;
    reg_pos[name] = reg;

    if (load)
        var_mem("lw", name, reg_names[reg], riscv);

    return reg;
}

void Controller::try_invalidate(const std::string name)
{
    if (is_allocvar(name))
        return;
    if (reg_pos[name].has_value())
    {
        reg_in_use[reg_pos[name].value()] = std::nullopt;
        reg_pos[name] = std::nullopt;
    }
}

bool is_label(const std::vector<std::string> &v)
{
    return v.size() == 1 && v[0].back() == ':';
}

void RISCV::to_string(std::string &str) const
{
    for (const auto &i : text)
    {
        int tabs = 1 - is_label(i);
        std::string content = "";
        for (int j = 0; j < i.size(); j++)
        {
            content += i[j];
            if (j >= 1 && j != i.size() -1)
                content += ",";
            content += " ";
        }
        if (start_with(content, "entry:"))
            content = "";
        new_line(str, content, tabs);
    }
}

void FuncRISCVINFO::init_save_reg()
{
    for (int i = 1; i < SAVED_REG_NUM; i++)
        save_pos["saved " + std::to_string(i)] = (i + 1) * 4;
}

void Controller::checkout(std::unordered_map<std::string, unsigned> &new_set, RISCV &riscv, bool l)
{
    std::unordered_map<std::string, unsigned> old_current_save = current_save;
    current_save.clear();
    for (auto i : new_set)
    {
        if (!(old_current_save.count(i.first) && old_current_save.at(i.first) == i.second && start_with(i.first, "saved ")))
        {
            alloc(i.first, riscv, false);
            load(i.first, riscv, l, i.second);
        }
        current_save.insert({i.first, i.second});
    }
}

bool Controller::has_set_label(std::string name) const
{
    return label_set.count(name);
}

void Controller::set_label(std::string name)
{
    label_set.insert(name);
}
