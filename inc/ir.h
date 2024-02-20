#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>

#include <riscv.h>
#include <list.h>

extern const std::unordered_set<std::string> op_name;
extern const std::unordered_set<std::string> end_of_block;
extern const std::unordered_map<std::string, std::string> lib_func_type;
extern const std::unordered_map<std::string, std::string> lib_func_decl;


class IRINFO;

class BaseIR {
    public:
        virtual ~BaseIR() = default;
        std::weak_ptr<IRINFO> info;
        virtual void to_string(std::string& str, const int tabs=0) const = 0;
        virtual void to_riscv(RISCV &riscv, Controller &cont) = 0;
        virtual void gather_super() = 0;
        virtual void alloc_preserve(bool in_while=true) = 0;
        virtual void print_super() = 0;
};

class ValueIR : public BaseIR {
    public:
        std::string op;
        std::vector<std::string> args;
        virtual void to_string(std::string& str, const int tabs=0) const;
        virtual void to_riscv(RISCV &riscv, Controller &cont);
        virtual void gather_super() {}
        virtual void alloc_preserve(bool in_while=true) {}
        virtual void print_super() {}
};

class BlockIR : public BaseIR {
    public:
        std::string name;
        std::unordered_map<std::string, unsigned> count;
        virtual void to_string(std::string& str, const int tabs=0) const = 0;
        virtual void to_riscv(RISCV &riscv, Controller &cont) = 0;
        virtual void gather_super() {}
};

class BaseBlockIR : public BlockIR {
    public:
        List<std::unique_ptr<ValueIR>> values;
        virtual void to_string(std::string& str, const int tabs=0) const;
        virtual void to_riscv(RISCV &riscv, Controller &cont);
        virtual void alloc_preserve(bool in_while=true);
        virtual void print_super() {}
};

class SuperBlockIR : public BlockIR {
    public:
        List<std::unique_ptr<BlockIR>> base_blocks;
        std::unordered_set<std::string> preserve;
        virtual void to_string(std::string& str, const int tabs=0) const {};
        virtual void to_riscv(RISCV &riscv, Controller &cont);
        virtual void alloc_preserve(bool in_while=true);
        virtual void print_super();
};

class FunctionIR : public BaseIR {
    public:
        FuncRISCVINFO func_riscv_info;
        std::string name;
        std::string return_type;
        std::vector<std::string> args;
        List<std::unique_ptr<BaseBlockIR>> base_blocks;
        std::unique_ptr<SuperBlockIR> super_block;
        virtual void to_string(std::string& str, const int tabs=0) const;
        virtual void to_riscv(RISCV &riscv, Controller &cont);
        virtual void gather_super();
        virtual void alloc_preserve(bool in_while=true);
        virtual void print_super();
};

class ProgramIR : public BaseIR {
    public:
        GlobRISCVINFO global_riscv_info;
        std::shared_ptr<IRINFO> program_info;
        std::vector<std::unique_ptr<ValueIR>> values;
        std::vector<std::unique_ptr<FunctionIR>> functions;
        virtual void to_string(std::string& str, const int tabs=0) const;
        virtual void to_riscv(RISCV &riscv, Controller &cont);
        virtual void gather_super();
        virtual void alloc_preserve(bool in_while=true);
        virtual void print_super();
};

class PartIR : public BaseIR {
    private:
        List<std::unique_ptr<ValueIR>> values;
        std::optional<std::pair<List<std::unique_ptr<ValueIR>>, std::string>> new_values = std::nullopt;
    public:
        void seal_next(std::weak_ptr<IRINFO> info);
        List<std::unique_ptr<BaseBlockIR>> blocks;
        virtual void to_string(std::string& str, const int tabs=0) const;
        virtual void to_riscv(RISCV &riscv, Controller &cont) {};
        void merge(std::unique_ptr<PartIR>& part, std::weak_ptr<IRINFO> info);
        void append(std::unique_ptr<ValueIR> value, std::weak_ptr<IRINFO> info);
        void append(const std::vector<std::string> args, std::weak_ptr<IRINFO> info);
        void seal_prev(const std::string name, const std::string type);
        void create_new_block(const std::string name);
        void substitute(const std::string& name1, const std::string& name2);
        List<std::unique_ptr<ValueIR>>& get_values() { return values; };
        virtual void gather_super() {}
        virtual void alloc_preserve(bool in_while=true) {}
        virtual void print_super() {}
};

class IRINFO {
    private:
        int level, time;
        std::unordered_map<std::string, unsigned> label_count;
        std::unordered_map<std::string, unsigned> tvar_count;
        std::unordered_map<std::string, unsigned> tvar_last_count;
        std::unordered_map<std::string, unsigned> tvar_time;
        std::unordered_map<std::string, unsigned> pvar_count;
        std::unordered_map<std::string, std::unordered_set<unsigned>> pvar_alloc;
        std::unordered_map<std::string, std::stack<unsigned>> pvar_stack;
        std::unordered_map<std::string, std::unordered_map<unsigned, std::optional<std::string>>> pvar_const;
        std::unordered_map<std::string, std::unordered_map<unsigned, unsigned>> pvar_level;
        std::unordered_map<std::string, std::unordered_map<unsigned, bool>> pvar_is_arg;
        std::unordered_map<std::string, std::unordered_map<unsigned, List<std::optional<unsigned>>>> pvar_type;
        std::unordered_map<std::string, std::string> func_type;
    public:
        IRINFO();
        virtual ~IRINFO() = default;
        std::string last_result;
        std::string current_state;
        std::string func_name;
        std::string get_var_name(const std::string ast_name) const;
        std::string allocate_var(const std::string info, const bool temp=true, const bool var=false, const bool is_arg=false, const List<std::optional<unsigned>> dims={});
        std::string allocate_label(const std::string info);
        void set_const(const std::string& name, const std::string& value);
        std::optional<std::string> get_const(const std::string& name) const;
        std::unique_ptr<PartIR> get_alloc(std::weak_ptr<IRINFO> info) const;
        void set_func(const std::string& name, const std::string& type);
        std::string get_func(const std::string& name) const;
        std::vector<std::optional<unsigned>> get_type(const std::string& name) const;
        bool is_arg(const std::string& name) const;
        void inc_level();
        void dec_level();
        std::unique_ptr<PartIR> start_func(const List<std::pair<std::string, List<std::optional<unsigned>>>>& args, std::vector<std::string>& result, std::weak_ptr<IRINFO> info);
        void end_func();
};

