#pragma once

#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>

#include <list.h>
#include <str.h>

#define IMM12_MAX 2048
#define REG_NUM 32
#define FREE_REG_NUM 13
#define SAVED_REG_NUM 12
#define ZERO_REG 0
#define A0_REG 10
#define T0_REG 5
#define T5_REG 30
#define T6_REG 31

extern const std::string reg_names[REG_NUM];
extern const int free_regs[FREE_REG_NUM];
extern const int saved_regs[SAVED_REG_NUM];

extern int regname_to_idx(const std::string name);

class RISCV
{
public:
    List<std::vector<std::string>> text;
    ~RISCV() = default;
    void to_string(std::string &str) const;
};

extern void safe_mem(const std::string op, const std::string reg_name, const int loc, RISCV &riscv, const std::string base = "fp");

class GlobRISCVINFO
{
private:
    friend class Controller;

public:
    std::unordered_map<std::string, std::string> global_var;
    std::unordered_map<std::string, std::string> func_name;
    ~GlobRISCVINFO() = default;
};

class FuncRISCVINFO
{
private:
    int mem_need;
    std::unordered_map<std::string, int> save_pos;
    friend class Controller;

public:
    void init_save_reg();
    int get_mem_need() const { return mem_need; }
    int get_save_pos(const std::string name) const {if(!save_pos.count(name)) std::cout<<name<<std::endl; return save_pos.at(name); }
    ~FuncRISCVINFO() = default;
};

class Controller
{
private:
    GlobRISCVINFO *glob = nullptr;
    FuncRISCVINFO *func = nullptr;
    std::optional<std::string> reg_in_use[32];
    std::unordered_map<std::string, std::optional<int>> reg_pos;
    std::unordered_set<std::string> label_set;
    int current_time;
    int last_used[REG_NUM];
    int find_lru();
    int find_reg(RISCV &riscv);

public:
    std::unordered_map<std::string, unsigned> current_save;
    int long_jump = 0;
    std::unordered_set<std::string> ptr;
    void clear(const std::vector<std::string>& args);
    void refresh(RISCV &riscv, bool save = true, std::vector<std::string> except = {});
    void transition(RISCV &riscv, std::string mode);
    void alloc(const std::string name, RISCV &riscv, bool reg = true, int size = 4);
    int load(const std::string name, RISCV &riscv, bool load = true, int specify = 0);
    void try_invalidate(const std::string name);
    void var_mem(const std::string op, const std::string name, const std::string reg_name, RISCV &riscv);
    void bind(const std::string reg, const std::string name);
    void save_back(int reg, RISCV &riscv, bool sync=false);
    void set_glob(GlobRISCVINFO *glob) { this->glob = glob; }
    void set_func(FuncRISCVINFO *func, const std::vector<std::string>& args) { this->func = func, clear(args); }
    void checkout(std::unordered_map<std::string, unsigned>& new_set, RISCV &riscv, bool load=true);
    void prepare_return(RISCV &riscv);
    bool has_set_label(std::string label) const;
    void set_label(std::string label);
    const GlobRISCVINFO *get_glob() const { return glob; }
    const FuncRISCVINFO *get_func() const { return func; }
};