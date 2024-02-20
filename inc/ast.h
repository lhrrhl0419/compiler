#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <optional>
#include <cassert>

#include <ir.h>
#include <list.h>

class BaseAST {
    public:
        virtual ~BaseAST() = default;
        virtual void to_string(std::string& str, const int tabs=0) const = 0;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const = 0;
};

class StmtAST : public BaseAST {
    public:
        std::string type;
        virtual void to_string(std::string &str, const int tabs = 0) const = 0;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const = 0;
};

class BlockAST : public StmtAST {
    public:
        List<std::unique_ptr<StmtAST>> stmts;
        virtual void to_string(std::string& str, const int tabs=0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class ExpAST : public BaseAST {
    public:
        std::string op = "";
        std::optional<std::string> value = std::nullopt;
        List<std::unique_ptr<ExpAST>> args;
        std::optional<std::string> arr_name = std::nullopt;
        bool side_effect;
        void try_eval(std::weak_ptr<IRINFO> info);
        std::unique_ptr<ExpAST> copy() const;
        virtual void to_string(std::string &str, const int tabs = 0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class FuncDefAST : public BaseAST {
    public:
        std::string func_type;
        std::string ident;
        std::unique_ptr<BlockAST> block;
        List<std::pair<std::string, List<std::optional<std::unique_ptr<ExpAST>>>>> args;
        virtual void to_string(std::string& str, const int tabs=0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class FuncFPAST : public BaseAST {
    public:
        std::string name;
        List<std::optional<std::unique_ptr<ExpAST>>> dims;
        virtual void to_string(std::string& str, const int tabs=0) const {};
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const { assert(0); };
};

class FuncFPsAST : public BaseAST {
    public:
        List<std::unique_ptr<FuncFPAST>> args;
        virtual void to_string(std::string& str, const int tabs=0) const {};
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const { assert(0); };
};

class InitAST : public BaseAST {
    public:
        std::optional<std::unique_ptr<ExpAST>> exp = std::nullopt;
        List<std::unique_ptr<InitAST>> inits;
        void try_eval(std::weak_ptr<IRINFO> info, const List<std::optional<unsigned>>& dims);
        List<std::unique_ptr<ExpAST>> exps;
        std::string to_ir_string(std::weak_ptr<IRINFO> info);
        virtual void to_string(std::string& str, const int tabs=0) const {};
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class DimAST : public BaseAST {
    public:
        List<std::optional<std::unique_ptr<ExpAST>>> dims;
        virtual void to_string(std::string &str, const int tabs = 0) const {};
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const { assert(0); };
};

class StmtsAST : public BaseAST {
    public:
        List<std::unique_ptr<StmtAST>> stmts;
        virtual void merge(std::unique_ptr<StmtsAST>& stmts2);
        virtual void to_string(std::string &str, const int tabs = 0) const {};
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const { assert(0); };
};

class StmtExpAST : public StmtAST {
    public:
        std::unique_ptr<ExpAST> exp;
        virtual void to_string(std::string &str, const int tabs = 0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class ReturnAST : public StmtAST {
    public:
        std::optional<std::unique_ptr<ExpAST>> exp;
        virtual void to_string(std::string &str, const int tabs = 0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class AssignAST : public StmtAST {
    public:
        std::string ident;
        List<std::unique_ptr<ExpAST>> dims;
        std::unique_ptr<ExpAST> exp;
        virtual void to_string(std::string &str, const int tabs = 0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class DefAST : public StmtAST {
    public:
        bool is_const = false;
        std::string ident;
        std::optional<std::unique_ptr<ExpAST>> exp = std::nullopt;
        List<std::unique_ptr<ExpAST>> dims;
        std::optional<std::unique_ptr<InitAST>> init = std::nullopt;
        virtual void to_string(std::string &str, const int tabs = 0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class IfAST : public StmtAST {
    public:
        std::unique_ptr<ExpAST> exp;
        std::unique_ptr<BlockAST> then_stmt;
        std::optional<std::unique_ptr<BlockAST>> else_stmt;
        virtual void to_string(std::string &str, const int tabs = 0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class WhileAST : public StmtAST {
    public:
        std::unique_ptr<ExpAST> exp;
        std::unique_ptr<BlockAST> stmt;
        virtual void to_string(std::string &str, const int tabs = 0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class ControlAST : public StmtAST {
    public:
        std::string type;
        virtual void to_string(std::string &str, const int tabs = 0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};

class CompUnitAST : public BaseAST {
    public:
        List<std::unique_ptr<DefAST>> var_def;
        List<std::unique_ptr<FuncDefAST>> func_def;
        virtual void to_string(std::string& str, const int tabs=0) const;
        virtual std::unique_ptr<BaseIR> to_ir(std::weak_ptr<IRINFO> info) const;
};
