#include <ir.h>
#include <str.h>
#include <cassert>
#include <iostream>
#include <queue>
#include <algorithm>

const std::unordered_set<std::string> op_name = {"add", "sub", "mul", "div", "mod", "and", "or", "eq", "ne", "lt", "gt", "le", "ge"};
const std::unordered_set<std::string> end_of_block = {"br", "jump", "ret"};
const std::unordered_map<std::string, std::string> lib_func_type = {
    {"getint", "int"},
    {"getch", "int"},
    {"getarray", "int"},
    {"putint", "void"},
    {"putch", "void"},
    {"putarray", "void"},
    {"starttime", "void"},
    {"stoptime", "void"},
};
const std::unordered_map<std::string, std::string> lib_func_decl = {
    {"getint", "decl @getint(): i32"},
    {"getch", "decl @getch(): i32"},
    {"getarray", "decl @getarray(*i32): i32"},
    {"putint", "decl @putint(i32)"},
    {"putch", "decl @putch(i32)"},
    {"putarray", "decl @putarray(i32, *i32)"},
    {"starttime", "decl @starttime()"},
    {"stoptime", "decl @stoptime()"},
};

int get_type_size(const std::string type)
{
    if (type == "i32")
        return 4;
    std::string result = type.substr(6, type.size() - 7);
    return std::stoi(result) * 4;
}

int getlog(int x)
{
    for(int i = 0; i < 32; i++)
        if((1 << i) == x)
            return i;
    return -1;
}

IRINFO::IRINFO()
{
    level = 0;
    for (auto const& it : lib_func_type)
        func_type[it.first] = it.second;
}

std::string IRINFO::get_var_name(const std::string ast_name) const
{
    if (is_tvar(ast_name))
        return ast_name;
    else
        return "@" + ast_name + "_" + std::to_string(pvar_stack.at(ast_name).top());
}

std::string IRINFO::allocate_var(const std::string info, const bool temp, const bool var, const bool is_arg, const List<std::optional<unsigned>> dims)
{
    if (!var)
    {
        std::string new_info = (temp ? "temp_" : "alloc_") + info;
        if (tvar_count.find(new_info) == tvar_count.end())
            tvar_count[new_info] = 0, tvar_last_count[new_info] = 0;
        std::string var_name = "%" + new_info + "_" + std::to_string(tvar_count[new_info]++);
        tvar_time[var_name] = time++;
        return var_name;
    }
    else
    {
        if (!pvar_count.count(info))
        {
            pvar_count[info] = 0;
            pvar_alloc[info] = std::unordered_set<unsigned>();
            pvar_stack[info] = std::stack<unsigned>();
            pvar_const[info] = std::unordered_map<unsigned, std::optional<std::string>>();
            pvar_level[info] = std::unordered_map<unsigned, unsigned>();
            pvar_is_arg[info] = std::unordered_map<unsigned, bool>();
            pvar_type[info] = std::unordered_map<unsigned, List<std::optional<unsigned>>>();
        }
        pvar_alloc[info].insert(pvar_count[info]);
        pvar_stack[info].push(pvar_count[info]);
        pvar_const[info][pvar_count[info]] = std::nullopt;
        pvar_level[info][pvar_count[info]] = level;
        pvar_is_arg[info][pvar_count[info]] = is_arg;
        pvar_type[info][pvar_count[info]] = List<std::optional<unsigned>>();
        if (!dims.empty())
            for (auto const& dim : dims)
                pvar_type[info][pvar_count[info]].push_back(dim);
        std::string var_name = "@" + info + "_" + std::to_string(pvar_count[info]++);
        return var_name;
    }
}

std::string IRINFO::allocate_label(const std::string info)
{
    if (label_count.find(info) == label_count.end())
        label_count[info] = 0;
    std::string label_name = "\%label_" + info + "_" + std::to_string(label_count[info]++);
    return label_name;
}

void IRINFO::set_const(const std::string& name, const std::string& value)
{
    pvar_const[name][pvar_stack[name].top()] = value;
}

std::optional<std::string> IRINFO::get_const(const std::string& name) const
{
    std::string ident = name.substr(1, name.rfind("_") - 1);
    return pvar_const.at(ident).at(pvar_stack.at(ident).top());
}

std::unique_ptr<PartIR> IRINFO::get_alloc(std::weak_ptr<IRINFO> info) const
{
    auto ir = std::make_unique<PartIR>();
    for (auto const& it : pvar_alloc)
    {
        for (auto const& it2 : it.second)
        {
            int level = pvar_level.at(it.first).at(it2);
            if (level <= 1 || pvar_const.at(it.first).at(it2).has_value() || pvar_is_arg.at(it.first).at(it2))
                continue;
            auto value = std::make_unique<ValueIR>();
            value->op = "alloc";
            value->args = {"@" + it.first + "_" + std::to_string(it2)};
            if (pvar_type.at(it.first).at(it2).empty())
                value->args.push_back("i32");
            else
            {
                int size = 1;
                for (auto const& dim : pvar_type.at(it.first).at(it2))
                    size *= dim.value();
                value->args.push_back("[i32, " + std::to_string(size) + "]");
            }
            ir->append(std::move(value), info);
        }
    }
    std::vector<std::pair<unsigned, std::unique_ptr<ValueIR>>> tvar_decls;
    for (auto const& it : tvar_count)
    {
        if (!is_allocvar("%" + it.first))
            for (int i = tvar_last_count.at(it.first); i < it.second; i++)
            {
                auto value = std::make_unique<ValueIR>();
                std::string var_name = "%" + it.first + "_" + std::to_string(i);
                value->op = "//!", value->args = {"decl", var_name, "i32"};
                tvar_decls.push_back({tvar_time.at(var_name), std::move(value)});
            }
        else
            for (int i = tvar_last_count.at(it.first); i < it.second; i++)
            {
                auto value = std::make_unique<ValueIR>();
                std::string var_name = "%" + it.first + "_" + std::to_string(i);
                value->op = "alloc", value->args = {var_name, "i32"};
                tvar_decls.push_back({tvar_time.at(var_name), std::move(value)});
            }
    }
    std::sort(tvar_decls.begin(), tvar_decls.end());
    for (auto& it : tvar_decls)
        ir->append(std::move(it.second), info);
    return ir;
}

void IRINFO::set_func(const std::string& name, const std::string& type)
{
    func_type[name] = type;
}

std::string IRINFO::get_func(const std::string& name) const
{
    return func_type.at(name);
}

std::vector<std::optional<unsigned>> IRINFO::get_type(const std::string& name) const
{
    std::string ident = name[0] == '@' ? name.substr(1, name.rfind("_") - 1) : name;
    auto result_list = pvar_type.at(ident).at(pvar_stack.at(ident).top());
    auto result_vector = std::vector<std::optional<unsigned>>();
    for (auto const& result : result_list)
        result_vector.push_back(result);
    return result_vector;
}

bool IRINFO::is_arg(const std::string& name) const
{
    std::string ident = name[0] == '@' ? name.substr(1, name.rfind("_") - 1) : name;
    return pvar_is_arg.at(ident).at(pvar_stack.at(ident).top()) && !pvar_type.at(ident).at(pvar_stack.at(ident).top()).empty();
}

void IRINFO::inc_level()
{
    level++;
}

void IRINFO::dec_level()
{
    for (auto& it : pvar_stack)
        if (it.second.size() && pvar_level[it.first][it.second.top()] == level)
            it.second.pop();
    level--;
}

std::unique_ptr<PartIR> IRINFO::start_func(const List<std::pair<std::string, List<std::optional<unsigned>>>>& args, std::vector<std::string>& result, std::weak_ptr<IRINFO> info)
{
    time = 0;
    std::unique_ptr<PartIR> ir = std::make_unique<PartIR>();
    for (auto const& arg : args)
    {
        std::string pname = allocate_var(arg.first, false, true, true, arg.second); 
        std::string tname = arg.second.empty() ? "\%arg_" + pname.substr(1) : pname;
        std::string type = arg.second.empty() ? "i32" : "*i32";
        result.push_back(tname + ": " + type);
        if (!arg.second.empty())
            continue;
        auto value = std::make_unique<ValueIR>();
        value->op = "alloc", value->args = {pname, type, "//!", "disgard"};
        ir->append(std::move(value), info);
        auto value2 = std::make_unique<ValueIR>();
        value2->op = "store", value2->args = {tname, pname, "//!", "disgard"};
        ir->append(std::move(value2), info);
    }
    return ir;
}

void IRINFO::end_func()
{
    pvar_alloc.clear();
    tvar_time.clear();
    for (auto& it : tvar_last_count)
        it.second = tvar_count[it.first];
}

void ProgramIR::to_string(std::string& str, const int tabs) const
{
    for (auto const& it : lib_func_decl)
        new_line(str, it.second, tabs);
    new_line(str);
    for (auto const& value : values)
        value->to_string(str, tabs);
    new_line(str);
    for (auto const& function : functions)
        function->to_string(str, tabs);
}

void ValueIR::to_string(std::string& str, const int tabs) const
{

    std::string instruciton = op;
    if (op_name.count(op))
        instruciton = args[0] + " = " + op + " " + args[1] + ", " + args[2];
    else if (op == "alloc" || op == "load")
        instruciton = args[0] + " = " + op + " " + args[1];
    else if (op == "global alloc")
    {
        instruciton = "global " + args[0] + " = alloc " + args[1] + ", " + args[2];
    }
    else if (op == "call_int")
    {
        instruciton = args[1] + " = call @" + args[0] + "(";
        for (int i = 2; i < args.size(); i++)
        {
            if (i > 2)
                instruciton += ", ";
            instruciton += args[i];
        }
        instruciton += ")";
    }
    else if (op == "call_void")
    {
        instruciton = "call @" + args[0] + "(";
        for (int i = 1; i < args.size(); i++)
        {
            if (i > 1)
                instruciton += ", ";
            instruciton += args[i];
        }
        instruciton += ")";
    }
    else if (op == "getelemptr" || op == "getptr")
        instruciton = args[0] + " = " + op + " " + args[1] + ", " + args[2];
    else
    {
        for (int i = 0; i < args.size(); i++)
        {
            if (i && args[i] != "//!")
                instruciton += ",";
            instruciton += " ";
            instruciton += args[i];
        }
    }
    if (instruciton.find("//!") == std::string::npos)
    {
        for (int i = 0; i < args.size(); i++)
        {
            if (args[i] == "//!")
            {
                instruciton += " //!";
                for (int j = i + 1; j < args.size(); j++)
                    instruciton += " " + args[j];
            }
        }
    }
    new_line(str, instruciton, tabs);
}

void FunctionIR::to_string(std::string& str, const int tabs) const
{
    add_tabs(str, tabs);
    str += "fun @" + name + "(";
    for (int i = 0; i < args.size(); i++)
    {
        if (i)
            str += ", ";
        str += args[i];
    }
    str += ")"; 
    if (return_type == "int")
        str += ": i32";
    str += " {";
    new_line(str);
    for (auto const& base_block : base_blocks)
        base_block->to_string(str, tabs);
    new_line(str, "}", tabs);
    new_line(str);
}

void BaseBlockIR::to_string(std::string& str, const int tabs) const
{
    new_line(str, name + ":", tabs);
    for (auto const& value : values)
        value->to_string(str, tabs + 1);
}

// only for debug
void PartIR::to_string(std::string& str, const int tabs) const
{
    std::cout<<"values:"<<std::endl;
    for (auto const& value : values)
        std::cout<<"\t"<<value->op<<std::endl;
    std::cout<<std::endl;
    std::cout<<"blocks:"<<std::endl;
    for (auto const& block : blocks)
    {
        std::cout<<"\t"<<block->name<<std::endl;
        for (auto const& value : block->values)
            std::cout<<"\t\t"<<value->op<<std::endl;
    }
    std::cout<<std::endl;
    if (new_values.has_value())
    {
        std::cout<<"new_values: "<<new_values.value().second<<std::endl;
        for (auto const& value : new_values.value().first)
            std::cout<<"\t"<<value->op<<std::endl;
    }
    std::cout<<std::endl;
}

void ProgramIR::to_riscv(RISCV &riscv, Controller &cont)
{
    cont.set_glob(&global_riscv_info);
    riscv.text.push_back({".data"});
    for (auto const &value : values)
    {
        std::string value_name = value->args[0];
        std::string riscv_name = "globl_" + value_name.substr(1);
        global_riscv_info.global_var.insert({value_name, riscv_name});
        riscv.text.push_back({".globl", riscv_name});
        riscv.text.push_back({riscv_name + ":"});
        if (value->args[2] == "undef")
            riscv.text.push_back({".zero", std::to_string(get_type_size(value->args[1]))});
        else if (value->args[2][0] != '{')
            riscv.text.push_back({".word", value->args[2]});
        else
        {
            std::string temp = value->args[2].substr(1, value->args[2].size() - 2);
            int start = 0, end = (temp.find(", ") == std::string::npos) ? temp.size() : temp.find(", ");
            int zero_num = 0;
            do
            {
                auto num = temp.substr(start, end - start);
                if (num == "0")
                    zero_num += 4;
                else
                {
                    if (zero_num)
                    {
                        riscv.text.push_back({".zero", std::to_string(zero_num)});
                        zero_num = 0;
                    }
                    riscv.text.push_back({".word", temp.substr(start, end - start)});
                }
                start = end + 2;
                end = (temp.find(", ", start) == std::string::npos) ? temp.size() : temp.find(", ", start);
            } while (start < temp.size());
            if (zero_num)
                riscv.text.push_back({".zero", std::to_string(zero_num)});
        }
    }
    riscv.text.push_back({""});
    riscv.text.push_back({".text"});
    for (auto const &it : lib_func_decl)
        global_riscv_info.func_name.insert({it.first, it.first});
    global_riscv_info.func_name.insert({"main", "main"});
    for (auto const &function : functions)
        if (function->name != "main")
            global_riscv_info.func_name.insert({function->name, "func_" + function->name});
    for (auto const& function : functions)
        function->to_riscv(riscv, cont);
}

void ValueIR::to_riscv(RISCV &riscv, Controller &cont)
{
    std::string ir = "#  ";
    this->to_string(ir);
    riscv.text.push_back({ir.substr(0, ir.size() - 1) + ":"});
    if (!args.empty() && args[args.size() - 1] == "disgard")
        return;
    if (op == "ret")
    {
        if (args.size())
        {
            cont.save_back(A0_REG, riscv);
            if (is_var(args[0]))
                cont.load(args[0], riscv, true, A0_REG);
            else
                riscv.text.push_back({"li", "a0", args[0]});
        }
        cont.refresh(riscv, false);
        cont.prepare_return(riscv);
        riscv.text.push_back({"lw", "ra", "-4(fp)"});
        riscv.text.push_back({"lw", "t6", "0(sp)"});
        riscv.text.push_back({"mv", "sp", "fp"});
        riscv.text.push_back({"mv", "fp", "t6"});
        riscv.text.push_back({"ret"});
    }
    else if (op == "alloc")
        cont.alloc(args[0], riscv, true, get_type_size(args[1]));
    else if (op == "br")
    {
        int reg;
        if (is_var(args[0]))
            reg = cont.load(args[0], riscv);
        else
            riscv.text.push_back({"li", "t6", args[0]}), reg = T6_REG;
        cont.try_invalidate(args[0]);
        cont.refresh(riscv); 
        std::string temp_label = "labellongjump_" + std::to_string(cont.long_jump++);
        riscv.text.push_back({"bnez", reg_names[reg], temp_label});
        riscv.text.push_back({"j", args[2].substr(1)});
        riscv.text.push_back({temp_label + ":"});
        riscv.text.push_back({"j", args[1].substr(1)});
    }
    else if (op == "jump")
    {
        cont.refresh(riscv);
        if (cont.has_set_label(args[0].substr(1)) || !start_with(args[0].substr(1), "label_while_cond"))
            riscv.text.push_back({"j", args[0].substr(1)});
        else
            riscv.text.push_back({"j", args[0].substr(1)+"_prepare"});
    }
    else if (start_with(op, "call"))
    {
        int with_return = (op == "call_int");
        int arg_num = args.size() - 1 - with_return;
        int pad_num = (4 - (arg_num % 4)) % 4;
        int size_need = (arg_num + pad_num) * 4;
        riscv.text.push_back({"li", "t6", std::to_string(size_need)});
        riscv.text.push_back({"sub", "sp", "sp", "t6"});
        std::string func_name = cont.get_glob()->func_name.at(args[0]);
        for (int i = 0; i < std::min(8, arg_num); i++)
        {
            if (is_num(args[i + 1 + with_return]))
                cont.save_back(A0_REG + i, riscv, true), riscv.text.push_back({"li", "a" + std::to_string(i), args[i + 1 + with_return]});
            else
                cont.load(args[i + 1 + with_return], riscv, true, regname_to_idx("a" + std::to_string(i)));
        }
        for (int i = 8; i < arg_num; i++)
        {
            if (is_num(args[i + 1 + with_return]))
                riscv.text.push_back({"li", "t6", args[i + 1 + with_return]});
            else
                cont.load(args[i + 1 + with_return], riscv, true, T6_REG);
            safe_mem("sw", "t6", -(i - 8) * 4, riscv, "sp");
        }
        for (int i=0;i<arg_num;i++)
            cont.try_invalidate(args[i + 1 + with_return]);
        cont.refresh(riscv, true);
        cont.transition(riscv, "sw");
        riscv.text.push_back({"call", func_name});
        riscv.text.push_back({"li", "t6", std::to_string(size_need)});
        riscv.text.push_back({"add", "sp", "sp", "t6"});
        cont.transition(riscv, "lw");
        cont.refresh(riscv);
        if (with_return)
            cont.bind("a0", args[1]);
    }
    else if (op == "getptr" || op == "getelemptr")
    {
        cont.ptr.insert(args[0]);
        if (is_num(args[2]))
            riscv.text.push_back({"li", "t6", std::to_string(std::stoi(args[2]) * 4)});
        else
        {
            int reg = cont.load(args[2], riscv);
            riscv.text.push_back({"li", "t6", "2"});
            riscv.text.push_back({"sll", "t6", reg_names[reg], "t6"});
        }
        int target_reg = cont.load(args[0], riscv, false);
        int ptr_reg;
        if (op == "getptr")
            ptr_reg = cont.load(args[1], riscv);
        else if (cont.get_glob()->global_var.count(args[1]))
            riscv.text.push_back({"la", "t5", cont.get_glob()->global_var.at(args[1])}), ptr_reg = T5_REG;
        else
        {
            int pos = cont.get_func()->get_save_pos(args[1]);
            riscv.text.push_back({"li", "t5", std::to_string(-pos)});
            riscv.text.push_back({"add", "t5", "t5", "fp"});
            ptr_reg = T5_REG;
        }
        riscv.text.push_back({"add", reg_names[target_reg], reg_names[ptr_reg], "t6"});
        cont.try_invalidate(args[2]);
    }
    else if (op == "load")
    {
        int reg1 = cont.load(args[0], riscv, false);
        int reg2 = cont.load(args[1], riscv);
        if (cont.ptr.count(args[1]))
            riscv.text.push_back({"lw", reg_names[reg1], "0(" + reg_names[reg2] + ")"});
        else
            riscv.text.push_back({"mv", reg_names[reg1], reg_names[reg2]});
        cont.try_invalidate(args[1]);
    }
    else if (op == "store")
    {
        if (args[0][0] != '{')
        {
            if (cont.ptr.count(args[1]))
            {
                int vreg;
                if (is_num(args[0]))
                    riscv.text.push_back({"li", "t6", args[0]}), vreg = T6_REG;
                else
                    vreg = cont.load(args[0], riscv);
                int reg = cont.load(args[1], riscv);
                riscv.text.push_back({"sw", reg_names[vreg], "0(" + reg_names[reg] + ")"});
            }
            else
            {
                int reg = cont.load(args[1], riscv, false);
                if (is_num(args[0]))
                    riscv.text.push_back({"li", reg_names[reg], args[0]});
                else
                {
                    int reg1 = cont.load(args[0], riscv);
                    riscv.text.push_back({"mv", reg_names[reg], reg_names[reg1]});
                }
            }
            cont.try_invalidate(args[0]);
        }
        else
        {
            std::string temp = args[0].substr(1, args[0].size() - 2);
            int size = 4;
            for (int i = 0; i < args[0].size(); i++)
                if (args[0][i] == ',')
                    size += 4;
            int start = 0, end = (temp.find(", ") == std::string::npos) ? temp.size() : temp.find(", ");
            int jump_num = 0;

            riscv.text.push_back({"li", "t6", std::to_string(-cont.get_func()->get_save_pos(args[1]))});
            riscv.text.push_back({"add", "t6", "t6", "fp"});
            for (int i = 0; i < size; i += 4)
            {
                std::string num = temp.substr(start, end - start);
                if (num != "undef")
                {
                    if (jump_num >= IMM12_MAX)
                    {
                        riscv.text.push_back({"li", "t5", std::to_string(jump_num)});
                        riscv.text.push_back({"add", "t6", "t6", "t5"});
                        jump_num = 0;
                    }
                    std::string num_reg;
                    if (num != "0")
                    {
                        riscv.text.push_back({"li", "t5", num});
                        num_reg = "t5";
                    }
                    else
                        num_reg = "zero";
                    riscv.text.push_back({"sw", num_reg, std::to_string(jump_num) + "(t6)"});
                }
                jump_num += 4;
                start = end + 2;
                end = (temp.find(", ", start) == std::string::npos) ? temp.size() : temp.find(", ", start);
            }
        }
        cont.try_invalidate(args[1]);
    }
    else if (op_name.count(op))
    {
        std::string lhs, rhs;

        if (is_var(args[1]))
            lhs = args[1];
        else
            lhs = "!t6";

        if (is_var(args[2]))
            rhs = args[2];
        else
            rhs = "!t6";

        if (op == "sub" && rhs == "!t6" && std::stoi(args[2]) != (1 << 31))
            op = "add", args[2] = std::to_string(-std::stoi(args[2]));
        if (op == "add" || op == "or" || op == "xor" || op == "and")
        {
            if (lhs == "!t6")
                std::swap(lhs, rhs), std::swap(args[1], args[2]);
            if (is_num(args[2]) && std::stoi(args[2]) >= -IMM12_MAX && std::stoi(args[2]) < IMM12_MAX)
            {
                int reg = cont.load(args[0], riscv, false);
                int lreg = cont.load(lhs, riscv);
                riscv.text.push_back({op + "i", reg_names[reg], reg_names[lreg], args[2]});
                cont.try_invalidate(lhs);
                return;
            }
        }
        else if (op == "mul" || op == "div")
        {
            if (lhs == "!t6" && op == "mul")
                std::swap(lhs, rhs), std::swap(args[1], args[2]);
            if (is_num(args[2]) && getlog(std::stoi(args[2])) != -1)
            {
                int reg = cont.load(args[0], riscv, false);
                int lreg = cont.load(lhs, riscv);
                int log = getlog(std::stoi(args[2]));
                if (log)
                {
                    riscv.text.push_back({"li", "t6", std::to_string(log)});
                    riscv.text.push_back({(op == "mul") ? "sll" : "sra", reg_names[reg], reg_names[lreg], "t6"});
                }
                else
                    riscv.text.push_back({"mv", reg_names[reg], reg_names[lreg]});
                cont.try_invalidate(lhs);
                return;
            }
        }
        else if (op == "eq" || op == "ne")
        {
            if (lhs == "!t6")
                std::swap(lhs, rhs), std::swap(args[1], args[2]);
            if (args[2] == "0")
            {
                int reg = cont.load(args[0], riscv, false);
                int lreg = cont.load(lhs, riscv);
                riscv.text.push_back({"s" + op + "z", reg_names[reg], reg_names[lreg]});
                cont.try_invalidate(lhs);
                return;
            }
        }

        if (lhs == "!t6")
        {
            if (args[1] == "0")
                lhs = "!zero";
            else
                riscv.text.push_back({"li", "t6", args[1]});
        }
        if (rhs == "!t6")
        {
            if (args[2] == "0")
                rhs = "!zero";
            else
                riscv.text.push_back({"li", "t6", args[2]});
        }

        std::string riscv_op_name;
        if (op == "mod")
            riscv_op_name = "rem";
        else if (op == "lt" || op == "gt")
            riscv_op_name = "s" + op;
        else if (op == "eq" || op == "ne")
            riscv_op_name = "xor";
        else if (op == "le")
            riscv_op_name = "sgt";
        else if (op == "ge")
            riscv_op_name = "slt";
        else
            riscv_op_name = op;
        int reg = cont.load(args[0], riscv, false);
        int lreg = cont.load(lhs, riscv);
        int rreg = cont.load(rhs, riscv);
        riscv.text.push_back({riscv_op_name, reg_names[reg], reg_names[lreg], reg_names[rreg]});

        if (op == "le" || op == "ge")
            riscv.text.push_back({"seqz", reg_names[reg], reg_names[reg]});
        if (op == "eq" || op == "ne")
            riscv.text.push_back({"s" + op + "z", reg_names[reg], reg_names[reg]});
        cont.try_invalidate(lhs);
        cont.try_invalidate(rhs);
    }
    else if (op == "//!")
    {
        if (args[0] == "decl")
            cont.alloc(args[1], riscv, false);
    }
}

void FunctionIR::to_riscv(RISCV &riscv, Controller &cont)
{
    cont.set_func(&func_riscv_info, args);
    func_riscv_info.init_save_reg();
    riscv.text.push_back({".globl", cont.get_glob()->func_name.at(name)});
    riscv.text.push_back({cont.get_glob()->func_name.at(name) + ":"});
    riscv.text.push_back({"li", "t6"});
    auto sp_it = std::prev(riscv.text.end());
    riscv.text.push_back({"sub", "sp", "sp", "t6"});
    riscv.text.push_back({"sw", "fp", "0(sp)"});
    riscv.text.push_back({"add", "fp", "sp", "t6"});
    riscv.text.push_back({"sw", "ra", "-4(fp)"});
    super_block->to_riscv(riscv, cont);
    int mem_need = ((func_riscv_info.get_mem_need() + 4 + 15) / 16) * 16;
    sp_it->push_back(std::to_string(mem_need));
    riscv.text.push_back({""});
}

void BaseBlockIR::to_riscv(RISCV &riscv, Controller &cont)
{
    if (start_with(name, "\%label_while_next"))
    riscv.text.push_back({name.substr(1)+"_act" + ":"});
    else
    riscv.text.push_back({name.substr(1) + ":"});
    for (auto const& value : values)
        value->to_riscv(riscv, cont);
}

void SuperBlockIR::to_riscv(RISCV &riscv, Controller &cont)
{
    std::unordered_map<std::string, unsigned> old_current_save;
    std::unordered_map<std::string, unsigned> new_current_save;
    std::vector<std::string> to_save;

    for (auto const &pair : cont.current_save)
        old_current_save.insert(pair);

    for (auto const &var : preserve)
    {
        if (cont.current_save.count(var))
            new_current_save.insert({var, cont.current_save.at(var)});
        else
            to_save.push_back(var);
    }

    for (auto const &var : to_save)
    {
        for (int i = 1; i < SAVED_REG_NUM; i++)
        {
            int idx = saved_regs[i];
            bool found = false;
            for (auto const &pair : new_current_save)
                if (pair.second == idx)
                    found = true;
            if (found)
                continue;
            new_current_save.insert({var, idx});
            break;
        }
    }

    for (int i=1;i<SAVED_REG_NUM;i++)
    {
        bool found = false;
        for (auto const& pair:new_current_save)
            if (pair.second==saved_regs[i])
                found = true;
        if (!found)
            for(auto const& pair:old_current_save)
                if (pair.second==saved_regs[i])
                    new_current_save.insert(pair);
    }

    auto next_name = (*base_blocks.begin())->name.substr(1);
    if (next_name != "entry")
    {
        riscv.text.push_back({next_name+"_prepare:"});
        cont.set_label(next_name);
    }

    std::string from="", to="", old_name, new_name;
    for (int i=1;i<SAVED_REG_NUM;i++)
    {
        for (auto& pair:old_current_save)
            if (pair.second==saved_regs[i])
                old_name=pair.first;
        from+=reg_names[saved_regs[i]]+" "+old_name+" ";
        for (auto& pair:new_current_save)
            if (pair.second==saved_regs[i])
                new_name=pair.first;
        to+=reg_names[saved_regs[i]]+" "+new_name+" ";
    }
    riscv.text.push_back({"#", "from", from, "to", to});

    cont.checkout(new_current_save, riscv, (*base_blocks.begin())->name != "\%entry");
    for (auto const& block : base_blocks)
        block->to_riscv(riscv, cont);
    if ((*base_blocks.begin())->name != "\%entry")
    riscv.text.push_back({"label_while_next_" + (*base_blocks.begin())->name.substr(18) + ":"});
    cont.checkout(old_current_save, riscv);
    if ((*base_blocks.begin())->name != "\%entry")
    riscv.text.push_back({"j","label_while_next_" + (*base_blocks.begin())->name.substr(18) + "_act"});
}

void ProgramIR::gather_super()
{
    for (auto& func : functions)
        func->gather_super();
}

std::unique_ptr<SuperBlockIR> get_super(std::unordered_map<std::string, std::unique_ptr<BaseBlockIR>>& map, std::string start, bool first=false)
{
    std::unordered_set<std::string> permit_next;
    auto super = std::make_unique<SuperBlockIR>();
    std::queue<std::string> q;
    q.push(start);
    while(!q.empty())
    {
        std::string cur = q.front();
        q.pop();

        if (cur.length() >= 17 && cur.substr(7, 10) == "while_next" && !permit_next.count(cur))
            continue;
        else if (start_with(cur, "\%label_exit") && !first)
            continue;
        else if (cur.length() >= 17 && cur.substr(7, 10) == "while_cond" && cur != start)
        {
            super->base_blocks.push_back(get_super(map, cur));
            std::string next_name = "\%label_while_next_" + cur.substr(18);
            q.push(next_name);
            permit_next.insert(next_name);
        }
        else
        {
            if (map.find(cur) == map.end())
                continue;
            auto block = std::move(map[cur]);
            map.erase(cur);
            auto last_v = block->values.rbegin();
            if ((*last_v)->op == "jump")
            {
                if ((*last_v)->args[0] != start)
                    q.push((*last_v)->args[0]);
            }
            else if ((*last_v)->op == "br")
                q.push((*last_v)->args[1]), q.push((*last_v)->args[2]);

            super->base_blocks.push_back(std::move(block));
        }
    }
    return super;
}

void FunctionIR::gather_super()
{
    std::unordered_map<std::string, std::unique_ptr<BaseBlockIR>> map;
    for (auto& block : base_blocks)
        map[block->name] = std::move(block);
    super_block = get_super(map, "\%entry", true);
}

void ProgramIR::alloc_preserve(bool in_while)
{
    for (auto& func : functions)
        func->alloc_preserve();
}

void FunctionIR::alloc_preserve(bool in_while)
{
    super_block->alloc_preserve(false);
}

void add_count(std::unordered_map<std::string, unsigned>& count, std::string key, unsigned value=1)
{
    if (count.find(key) == count.end())
        count[key] = 0;
    count[key] += value;
}

void check_and_add_count(std::unordered_map<std::string, unsigned>& count, std::string key, unsigned value=1)
{
    if (is_allocvar(key))
        add_count(count, key, value);
}

void BaseBlockIR::alloc_preserve(bool in_while)
{
    for (const auto& value : values)
    {
        if (!value->args.empty() && value->args[value->args.size() - 1] == "disgard")
            continue;
        else if (value->op == "//!")
            continue;
        else if (value->op == "ret" && value->args.size())
            check_and_add_count(count, value->args[0]);
        else if (value->op == "br")
            check_and_add_count(count, value->args[0]);
        else if (value->op == "load")
        {
            check_and_add_count(count, value->args[0]);
            check_and_add_count(count, value->args[1]);
        }
        else if (value->op == "store" && value->args[0][0] != '{')
        {
            check_and_add_count(count, value->args[0]);
            check_and_add_count(count, value->args[1]);
        }
        else if (op_name.count(value->op))
        {
            check_and_add_count(count, value->args[0]);
            check_and_add_count(count, value->args[1]);
            check_and_add_count(count, value->args[2]);
        }
        else if (start_with(value->op, "call"))
        {
            for (int i = 1; i < value->args.size(); i++)
                check_and_add_count(count, value->args[i]);
        }
        else if (value->op == "getptr" || value->op == "getelemptr")
            check_and_add_count(count, value->args[2]);
    }
}

void SuperBlockIR::alloc_preserve(bool in_while)
{
    for (auto& block : base_blocks)
    {
        block->alloc_preserve();
        for (auto& pair : block->count)
            add_count(count, pair.first);
    }

    std::vector<std::pair<std::string, unsigned>> vec;
    for (auto& pair : count)
        vec.push_back({pair.first, pair.second});
    std::sort(vec.begin(), vec.end(), [](const std::pair<std::string, unsigned>& a, const std::pair<std::string, unsigned>& b) { return a.second > b.second; });
    for (int i = 0; i < std::min(SAVED_REG_NUM-1, (int)vec.size()); i++)
    {
        // std::cout<<vec[i].first<<" "<<vec[i].second<<std::endl;
        if (vec[i].second > (in_while ? 0 : 1))
            preserve.insert(vec[i].first);
    }
    
    // for (auto s : preserve)
        // std::cout<<s<<std::endl;
    // std::cout<<std::endl;
}

void ProgramIR::print_super()
{
    for (auto& func : functions)
    {
        std::cout<<"function: "<<func->name<<std::endl<<std::endl;
        func->print_super();
    }
}

void FunctionIR::print_super()
{
    super_block->print_super();
}

void SuperBlockIR::print_super()
{
    std::cout<<"super:"<<std::endl;
    for (auto& block : base_blocks)
        std::cout<<"\t"<<block->name<<std::endl;
    std::cout<<std::endl;
    for (auto& pre : preserve)
        std::cout<<"\t"<<pre<<std::endl;
    std::cout<<std::endl;
    for (auto& block : base_blocks)
        block->print_super();
}