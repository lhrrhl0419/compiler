#include <ast.h>
#include <str.h>
#include <iostream>
#include <cassert>

std::string value_exp_to_ir(const std::unique_ptr<ExpAST>& exp, std::unique_ptr<PartIR>& part_ir, std::weak_ptr<IRINFO> info)
{
    assert(exp->value.has_value());
    if (is_num(exp->value.value()))
        return exp->value.value();
    else
    {
        std::string name = info.lock()->get_var_name(exp->value.value());
        if (info.lock()->get_const(name).has_value())
            return info.lock()->get_const(name).value();
        else if (!info.lock()->get_type(name).empty())
        {
            std::string op = info.lock()->is_arg(name) ? "getptr" :"getelemptr";
            std::string ret_name = info.lock()->allocate_var(op);
            part_ir->append({op, ret_name, name, "0"}, info);
            return ret_name;
        }
        else if (is_allocvar(name))
        {
            if (info.lock()->is_arg(name))
                return name;
            std::string tname = info.lock()->allocate_var("load");
            part_ir->append({"load", tname, name}, info);
            return tname;
        }
        else
            return name;
    }
}

std::optional<std::string> try_cal_str_value(const std::string &str, std::weak_ptr<IRINFO> info)
{
    if (is_num(str))
        return str;
    std::string name = info.lock()->get_var_name(str);
    return info.lock()->get_const(name);
}

std::optional<std::string> try_cal_value_exp(const std::unique_ptr<ExpAST> &exp, std::weak_ptr<IRINFO> info)
{
    return try_cal_str_value(exp->value.value(), info);
}

void PartIR::merge(std::unique_ptr<PartIR> &part, std::weak_ptr<IRINFO> info)
{
    blocks.merge(part->blocks);
    if (new_values.has_value())
    {
        new_values.value().first.merge(part->values);
        if (!new_values.value().first.empty() && end_of_block.count(new_values.value().first.rbegin()->get()->op))
            seal_next(info);
    }
    else
        if (values.empty() || !end_of_block.count(values.rbegin()->get()->op))
            values.merge(part->values);
    if (part->new_values.has_value())
        new_values = std::make_pair(std::move(part->new_values.value().first), std::move(part->new_values.value().second));
}

void PartIR::seal_next(std::weak_ptr<IRINFO> info)
{
    if (!new_values.has_value())
        return;
    auto next_block = std::make_unique<BaseBlockIR>();
    next_block->name = new_values.value().second;
    next_block->values.merge(new_values.value().first);
    if (next_block->values.empty() || !end_of_block.count(next_block->values.rbegin()->get()->op))
    {
        auto ret = std::make_unique<ValueIR>();
        ret->op = "jump";
        ret->args = {"\%labelexit_" + info.lock()->func_name};
        next_block->values.push_back(std::move(ret));
    }
    blocks.push_back(std::move(next_block));
    new_values = std::nullopt;
}

void PartIR::append(std::unique_ptr<ValueIR> value, std::weak_ptr<IRINFO> info)
{
    std::string op = value->op;
    if (new_values.has_value())
    {
        new_values.value().first.push_back(std::move(value));
        if (end_of_block.count(op))
            seal_next(info);
    }
    else if (values.empty() || !end_of_block.count(values.rbegin()->get()->op))
        values.push_back(std::move(value));
}

void PartIR::append(const std::vector<std::string> args, std::weak_ptr<IRINFO> info)
{
    auto value = std::make_unique<ValueIR>();
    value->op = args[0];
    value->args = {};
    for (int i = 1; i < args.size(); i++)
        value->args.push_back(args[i]);
    append(std::move(value), info);
}

void PartIR::seal_prev(const std::string name, const std::string type)
{
    auto prev_block = std::make_unique<BaseBlockIR>();
    prev_block->name = name;
    prev_block->values.merge(values);
    if (prev_block->values.empty() || !end_of_block.count(prev_block->values.rbegin()->get()->op))
    {
        auto ret = std::make_unique<ValueIR>();
        ret->op = "ret";
        if (type == "int")
            ret->args.push_back("0");
        prev_block->values.push_back(std::move(ret));
    }
    blocks.push_front(std::move(prev_block));
}

void PartIR::create_new_block(const std::string name)
{
    assert(!new_values.has_value());
    new_values = std::make_pair(List<std::unique_ptr<ValueIR>>(), name);
}

void PartIR::substitute(const std::string& name1, const std::string& name2)
{
    for (auto &value : values)
        if (value->op == "jump" || value->op == "br")
            for (auto &arg : value->args)
                if (arg == name1)
                    arg = name2;
    if (new_values.has_value())
        for (auto &value : new_values.value().first)
            if (value->op == "jump" || value->op == "br")
                for (auto &arg : value->args)
                    if (arg == name1)
                        arg = name2;
    for (auto &block : blocks)
        for (auto &value : block->values)
            if (value->op == "jump" || value->op == "br")
                for (auto &arg : value->args)
                    if (arg == name1)
                        arg = name2;
}

void CompUnitAST::to_string(std::string& str, const int tabs) const
{
    start_class(str, "CompUnit", tabs);
    for (auto & var_def : var_def)
    {
        add_tabs(str, tabs + 1);
        str += "var_def: ";
        var_def->to_string(str, tabs + 2);
    }
    for (auto & func_def : func_def)
    {
        add_tabs(str, tabs + 1);
        str += "func_def: ";
        func_def->to_string(str, tabs + 2);
    }
    end_class(str, "CompUnit", tabs);
}

void FuncDefAST::to_string(std::string& str, const int tabs) const
{
    start_class(str, "FuncDef", tabs);
    new_line(str, "func_type: " + func_type, tabs + 1);
    new_line(str, "ident: " + ident, tabs + 1);
    add_tabs(str, tabs + 1);
    str += "block: ";
    block->to_string(str, tabs + 2);
    end_class(str, "FuncDef", tabs);
}

void BlockAST::to_string(std::string& str, const int tabs) const
{
    start_class(str, "Block", tabs);
    for (auto & stmt : stmts)
    {
        add_tabs(str, tabs + 1);
        str += "stmt: ";
        stmt->to_string(str, tabs + 2);
    }
    end_class(str, "Block", tabs);
}

void StmtExpAST::to_string(std::string& str, const int tabs) const
{
    start_class(str, "StmtExp", tabs);
    add_tabs(str, tabs + 1);
    str += "exp: ";
    exp->to_string(str, tabs + 2);
    end_class(str, "StmtExp", tabs);
}

void ReturnAST::to_string(std::string& str, const int tabs) const
{
    start_class(str, "Return", tabs);
    if (exp.has_value())
    {
        add_tabs(str, tabs + 1);
        str += "exp: ";
        exp.value()->to_string(str, tabs + 2);
    }
    end_class(str, "Return", tabs);
}

void AssignAST::to_string(std::string& str, const int tabs) const
{
    start_class(str, "Assign", tabs);
    new_line(str, "ident: " + ident, tabs + 1);
    add_tabs(str, tabs + 1);
    str += "exp: ";
    exp->to_string(str, tabs + 2);
    end_class(str, "Assign", tabs);
}

void DefAST::to_string(std::string& str, const int tabs) const
{
    start_class(str, "Def", tabs);
    new_line(str, "ident: " + ident, tabs + 1);
    if (exp.has_value())
    {
        add_tabs(str, tabs + 1);
        str += "exp: ";
        exp.value()->to_string(str, tabs + 2);
    }
    end_class(str, "Def", tabs);
}

void ExpAST::to_string(std::string& str, const int tabs) const
{
    start_class(str, "Exp", tabs);
    if (value.has_value())
        new_line(str, "value: " + value.value(), tabs + 1);
    else
    {
        new_line(str, "op: " + op, tabs + 1);
        for (auto & arg : args)
        {
            add_tabs(str, tabs + 1);
            str += "arg: ";
            arg->to_string(str, tabs + 2);
        }
    }
    end_class(str, "Exp", tabs);
}

void IfAST::to_string(std::string& str, const int tabs) const
{
    start_class(str, "If", tabs);
    add_tabs(str, tabs + 1);
    str += "cond: ";
    exp->to_string(str, tabs + 2);
    add_tabs(str, tabs + 1);
    str += "then: ";
    then_stmt->to_string(str, tabs + 2);
    if (else_stmt.has_value())
    {
        add_tabs(str, tabs + 1);
        str += "else: ";
        else_stmt.value()->to_string(str, tabs + 2);
    }
    end_class(str, "If", tabs);
}

void WhileAST::to_string(std::string &str, const int tabs) const
{
    start_class(str, "While", tabs);
    add_tabs(str, tabs + 1);
    str += "cond: ";
    exp->to_string(str, tabs + 2);
    add_tabs(str, tabs + 1);
    str += "then: ";
    stmt->to_string(str, tabs + 2);
    end_class(str, "While", tabs);
}

void ControlAST::to_string(std::string &str, const int tabs) const
{
    start_class(str, "Control", tabs);
    new_line(str, "type: " + type, tabs + 1);
    end_class(str, "Control", tabs);
}

std::unique_ptr<BaseIR> CompUnitAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    auto result = std::make_unique<ProgramIR>();
    result->program_info = std::make_unique<IRINFO>();
    result->program_info->current_state = "global def";
    auto part = std::make_unique<PartIR>();
    for (auto & var_def_ : var_def)
    {
        auto temp = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(var_def_->to_ir(result->program_info).release()));
        part->merge(temp, info);
    }
    result->program_info->current_state = "";
    for (auto &value : part->get_values())
        result->values.push_back(std::unique_ptr<ValueIR>(value.release()));
    for (auto & func_def_ : func_def)
        result->program_info->set_func(func_def_->ident, func_def_->func_type);
    for (auto & func_def_ : func_def)
        result->functions.push_back(std::unique_ptr<FunctionIR>(dynamic_cast<FunctionIR*>(func_def_->to_ir(result->program_info).release())));
    return result;
}

std::unique_ptr<BaseIR> FuncDefAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    info.lock()->inc_level();
    info.lock()->func_name = ident;
    auto result = std::make_unique<FunctionIR>();
    List<std::pair<std::string, List<std::optional<unsigned>>>> num_args;
    for (auto &arg : args)
    {
        List<std::optional<unsigned>> num_dims;
        for (auto &dim : arg.second)
        {
            if (dim.has_value())
            {
                dim.value()->try_eval(info);
                assert(dim.value()->value.has_value() && is_num(dim.value()->value.value()));
                num_dims.push_back(std::stoi(dim.value()->value.value()));
            }
            else
                num_dims.push_back(std::nullopt);
        }
        num_args.push_back(std::make_pair(arg.first, num_dims));
    }
    auto prealloc_ir = info.lock()->start_func(num_args, result->args, info);
    auto part_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR*>(block->to_ir(info).release()));
    result->return_type = func_type;
    auto alloc_ir = info.lock()->get_alloc(info);
    alloc_ir->merge(prealloc_ir, info);
    alloc_ir->merge(part_ir, info);
    result->name = ident;
    alloc_ir->seal_prev("\%entry", func_type);
    alloc_ir->seal_next(info);
    alloc_ir->create_new_block("\%labelexit_" + ident);
    if (func_type == "int")
        alloc_ir->append({"ret", "0"}, info);
    else
        alloc_ir->append({"ret"}, info);
    result->base_blocks.merge(alloc_ir->blocks);
    info.lock()->end_func();
    info.lock()->dec_level();
    return std::unique_ptr<BaseIR>(dynamic_cast<BaseIR*>(result.release()));
}

std::unique_ptr<BaseIR> BlockAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    info.lock()->inc_level();
    auto result = std::make_unique<PartIR>();
    for (auto & stmt : stmts)
    {
        std::unique_ptr<PartIR> stmt_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR*>(stmt->to_ir(info).release()));
        result->merge(stmt_ir, info);
    }
    info.lock()->dec_level();
    return result;
}

std::unique_ptr<BaseIR> StmtExpAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    exp->try_eval(info);
    auto result_part = std::make_unique<PartIR>();
    if (!exp->value.has_value())
    {
        std::unique_ptr<PartIR> exp_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(exp->to_ir(info).release()));
        result_part->merge(exp_ir, info);
    }
    return result_part;
}

std::unique_ptr<BaseIR> ReturnAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    if (exp.has_value())
        exp.value()->try_eval(info);
    auto result_part = std::make_unique<PartIR>();
    if (exp.has_value())
    {
        std::string arg;
        if (exp.value()->value.has_value())
            arg = value_exp_to_ir(exp.value(), result_part, info);
        else
        {
            std::unique_ptr<PartIR> exp_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(exp.value()->to_ir(info).release()));
            result_part->merge(exp_ir, info);
            arg = info.lock()->last_result;
        }
        result_part->append({"ret", arg}, info);
    }
    else
        result_part->append({"ret"}, info);
    return result_part;
}

std::unique_ptr<BaseIR> ExpAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    auto result = std::make_unique<PartIR>();
    auto shared_info = info.lock();
    assert(!value.has_value());

    List<std::string> arg_names;
    List<std::optional<std::unique_ptr<PartIR>>> arg_irs;
    auto value_ir = std::make_unique<ValueIR>();
    std::string current = shared_info->allocate_var(op);

    for (auto & arg : args)
    {
        if (arg->value.has_value())
        {
            arg_irs.push_back(std::nullopt);
            arg_names.push_back(value_exp_to_ir(arg, result, info));
        }
        else
        {
            arg_irs.push_back(std::unique_ptr<PartIR>(dynamic_cast<PartIR*>(arg->to_ir(info).release())));
            arg_names.push_back(shared_info->last_result);
        }
    }

    if ((op == "and" || op == "or") && args.rbegin()->get()->side_effect)
    {
        if (arg_irs.begin()->has_value())
            result->merge(arg_irs.begin()->value(), info);

        std::string comp_name = shared_info->allocate_label(op+"_comp"), lazy_name = shared_info->allocate_label(op+"_lazy"), next_name = shared_info->allocate_label(op+"_next");
        std::string alloc_name = shared_info->allocate_var(op+"_alloc", false);

        if (op == "and")
            result->append({"br", *arg_names.begin(), comp_name, lazy_name}, info);
        else
            result->append({"br", *arg_names.begin(), lazy_name, comp_name}, info);

        result->create_new_block(lazy_name);
        result->append({"store", std::to_string(int(op == "or")), alloc_name}, info);
        result->append({"jump", next_name}, info);

        result->create_new_block(comp_name);
        if (arg_irs.rbegin()->has_value())
            result->merge(arg_irs.rbegin()->value(), info);
        if (is_num(*arg_names.rbegin()))
            result->append({"store", (*arg_names.rbegin() == "0") ? "0" : "1", alloc_name}, info);
        else
        {
            auto boolize_name = shared_info->allocate_var(op+"_boolize");
            result->append({"ne", boolize_name, *arg_names.rbegin(), "0"}, info);
            result->append({"store", boolize_name, alloc_name}, info);
        }

        result->append({"jump", next_name}, info);
        result->create_new_block(next_name);
        result->append({"load", current, alloc_name}, info);
    }
    else if (op == "and" || op == "or")
    {
        for (auto & arg_ir : arg_irs)
            if (arg_ir.has_value())
                result->merge(arg_ir.value(), info);
        auto boolize_name_1 = shared_info->allocate_var(op+"_boolize");
        if (is_num(*arg_names.begin()))
            boolize_name_1 = (*arg_names.begin() == "0") ? "0" : "1";
        else
            result->append({"ne", boolize_name_1, *arg_names.begin(), "0"}, info);
        auto boolize_name_2 = shared_info->allocate_var(op+"_boolize");
        if (is_num(*arg_names.rbegin()))
            boolize_name_2 = (*arg_names.rbegin() == "0") ? "0" : "1";
        else
            result->append({"ne", boolize_name_2, *arg_names.rbegin(), "0"}, info);
        result->append({op, current, boolize_name_1, boolize_name_2}, info);
    }
    else if (start_with(op, "func"))
    {
        for (auto & arg_ir : arg_irs)
            if (arg_ir.has_value())
                result->merge(arg_ir.value(), info);
        auto func_name = op.substr(5);
        value_ir->args = {op.substr(5)};
        if (info.lock()->get_func(func_name) == "int")
            value_ir->op = "call_int", value_ir->args.push_back(current);
        else if (info.lock()->get_func(func_name) == "void")
            value_ir->op = "call_void";
        for (auto & arg_name : arg_names)
            value_ir->args.push_back(arg_name);
        result->append(std::unique_ptr<ValueIR>(value_ir.release()), info);
    }
    else if (start_with(op, "at"))
    {
        if (arg_irs.begin()->has_value())
            result->merge(arg_irs.begin()->value(), info);
        std::string parr_name = info.lock()->get_var_name(arr_name.value());
        std::string getptrname = info.lock()->allocate_var("atptr");
        result->append({shared_info->is_arg(parr_name) ? "getptr" : "getelemptr", getptrname, parr_name, *arg_names.begin()}, info);
        if (op == "at")
            result->append({"load", current, getptrname}, info);
        else
            current = getptrname;
    }
    else
    {
        for (auto & arg_ir : arg_irs)
            if (arg_ir.has_value())
                result->merge(arg_ir.value(), info);
        value_ir->op = op;
        value_ir->args = {current, *arg_names.begin(), *arg_names.rbegin()};
        result->append(std::unique_ptr<ValueIR>(value_ir.release()), info);
    }

    shared_info->last_result = current;
    return result;
}

std::unique_ptr<BaseIR> AssignAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    exp->try_eval(info);
    auto result_part = std::make_unique<PartIR>();
    auto result = std::make_unique<ValueIR>();
    std::string name = info.lock()->get_var_name(ident);
    result->op = "store";
    if (exp->value.has_value())
        result->args.push_back(value_exp_to_ir(exp, result_part, info));
    else
    {
        std::unique_ptr<PartIR> exp_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(exp->to_ir(info).release()));
        result_part->merge(exp_ir, info);
        result->args.push_back(info.lock()->last_result);
    }
    if (dims.empty())
        result->args.push_back(name);
    else
    {
        auto at_exp = std::make_unique<ExpAST>();
        at_exp->op = "at_woload";
        at_exp->arr_name = ident;
        for (const auto &dim : dims)
            at_exp->args.push_back(dim->copy());
        at_exp->try_eval(info);
        auto part = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(at_exp->to_ir(info).release()));
        result_part->merge(part, info);
        result->args.push_back(info.lock()->last_result);
    }
    result_part->append(std::move(result), info);
    return result_part;
}

std::unique_ptr<BaseIR> InitAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    assert(0);
}

std::unique_ptr<BaseIR> DefAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    if (exp.has_value())
        exp.value()->try_eval(info);
    auto result_part = std::make_unique<PartIR>();
    auto num_dims = List<std::optional<unsigned>>();
    for (auto &exp : dims)
    {
        exp->try_eval(info);
        assert(exp->value.has_value() && is_num(exp->value.value()));
        num_dims.push_back(std::stoi(exp->value.value()));
    }
    std::string name = info.lock()->allocate_var(ident, false, true, false, num_dims);
    if (is_const && exp.has_value() && exp.value()->value.has_value() && is_num(exp.value()->value.value()))
    {
        info.lock()->set_const(ident, exp.value()->value.value());
        return result_part;
    }
    else if (exp.has_value())
    {
        auto result2 = std::make_unique<ValueIR>();
        result2->op = (info.lock()->current_state == "global def") ? "global alloc" : "store";
        std::string value;
        if (exp.value()->value.has_value())
            value = value_exp_to_ir(exp.value(), result_part, info);
        else
        {
            std::unique_ptr<PartIR> exp_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(exp.value()->to_ir(info).release()));
            result_part->merge(exp_ir, info);
            value = info.lock()->last_result;
        }
        if (info.lock()->current_state == "global def")
            result2->args = {name, "i32", value};
        else
            result2->args = {value, name};
        result_part->append(std::move(result2), info);
    }
    else if (num_dims.empty())
    {
        if (info.lock()->current_state == "global def")
            result_part->append({"global alloc", name, "i32", "undef"}, info);
    }
    else
    {
        int size = 1;
        for (auto &dim : num_dims)
            size *= dim.value();
        if (init.has_value())
            init.value()->try_eval(info, num_dims);
        if (info.lock()->current_state == "global def")
            result_part->append({"global alloc", name, "[i32, " + std::to_string(size) + "]", init.has_value() ? init.value()->to_ir_string(info) : "undef"}, info);
        else
        {
            if (init.has_value())
            {
                int cur = -1;
                auto store_part = std::make_unique<PartIR>();
                for (auto &e : init.value()->exps)
                {
                    std::string result;
                    cur++;
                    if (e->value.has_value())
                    {
                        if (try_cal_str_value(e->value.value(), info).has_value())
                            continue;
                        else
                            result = value_exp_to_ir(e, store_part, info);
                    }
                    else
                    {
                        auto p = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(e->to_ir(info).release()));
                        result_part->merge(p, info);
                        result = info.lock()->last_result;
                    }
                    std::string temp = info.lock()->allocate_var("getelemptr");
                    store_part->append({"getelemptr", temp, name, std::to_string(cur)}, info);
                    store_part->append({"store", result, temp}, info);
                }
                result_part->append({"store", init.value()->to_ir_string(info), name}, info);
                result_part->merge(store_part, info);
            }
        }
    }
    return result_part;
}

std::unique_ptr<BaseIR> IfAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    exp->try_eval(info);
    auto result_part = std::make_unique<PartIR>();
    std::string arg;
    if (exp->value.has_value())
        arg = value_exp_to_ir(exp, result_part, info);
    else
    {
        std::unique_ptr<PartIR> exp_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(exp->to_ir(info).release()));
        result_part->merge(exp_ir, info);
        arg = info.lock()->last_result;
    }
    std::string then_name = info.lock()->allocate_label("if_then"), else_name = info.lock()->allocate_label("if_else"), next_name = info.lock()->allocate_label("if_next");
    result_part->append({"br", arg, then_name, else_stmt.has_value() ? else_name : next_name}, info);

    result_part->create_new_block(then_name);
    auto then_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(then_stmt->to_ir(info).release()));
    result_part->merge(then_ir, info);
    result_part->append({"jump", next_name}, info);

    if (else_stmt.has_value())
    {
        result_part->create_new_block(else_name);
        auto else_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(else_stmt.value()->to_ir(info).release()));
        result_part->merge(else_ir, info);
        result_part->append({"jump", next_name}, info);
    }

    result_part->create_new_block(next_name);
    return result_part;
}

std::unique_ptr<BaseIR> WhileAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    exp->try_eval(info);
    std::string cond_name = info.lock()->allocate_label("while_cond"), then_name = info.lock()->allocate_label("while_then"), next_name = info.lock()->allocate_label("while_next");
    auto result_part = std::make_unique<PartIR>();
    result_part->append({"jump", cond_name}, info);
    result_part->create_new_block(cond_name);

    std::string arg;
    if (exp->value.has_value())
        arg = value_exp_to_ir(exp, result_part, info);
    else
    {
        std::unique_ptr<PartIR> exp_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(exp->to_ir(info).release()));
        result_part->merge(exp_ir, info);
        arg = info.lock()->last_result;
    }
    result_part->append({"br", arg, then_name, next_name}, info);

    result_part->create_new_block(then_name);
    auto then_ir = std::unique_ptr<PartIR>(dynamic_cast<PartIR *>(stmt->to_ir(info).release()));
    result_part->merge(then_ir, info);
    result_part->append({"jump", cond_name}, info);

    result_part->create_new_block(next_name);

    result_part->substitute("continue", cond_name);
    result_part->substitute("break", next_name);

    return result_part;
}

std::unique_ptr<BaseIR> ControlAST::to_ir(std::weak_ptr<IRINFO> info) const
{
    auto part = std::make_unique<PartIR>();
    part->append({"jump", type}, info);
    return part;
}

void StmtsAST::merge(std::unique_ptr<StmtsAST>& stmts2)
{
    stmts.merge(stmts2->stmts);
}

void ExpAST::try_eval(std::weak_ptr<IRINFO> info)
{
    if (value.has_value()) 
    {
        side_effect = false;
        auto result = try_cal_str_value(value.value(), info);
        if (result.has_value())
            value = result.value();
        return;
    }
    if (start_with(op, "at"))
    {
        assert(arr_name.has_value());
        side_effect = true;
        auto dims = info.lock()->get_type(arr_name.value());
        if (dims.size() != args.size())
            op = "at_woload";
        std::vector<std::unique_ptr<ExpAST>> muls;
        int i = 0;
        for (auto & arg : args)
        {
            auto mul = std::make_unique<ExpAST>();
            auto s = std::make_unique<ExpAST>();
            mul->op = "mul";
            int size = 1;
            for (int j = i + 1; j < dims.size(); j++)
                size *= dims[j].value();
            s->value = std::to_string(size);
            mul->args.push_back(std::move(arg));
            mul->args.push_back(std::move(s));
            muls.push_back(std::move(mul));
            i++;
        }
        std::unique_ptr<ExpAST> new_value = std::move(muls[0]);
        for (int i = 1; i < muls.size(); i++)
        {
            auto add = std::make_unique<ExpAST>();
            add->op = "add";
            add->args.push_back(std::move(new_value));
            add->args.push_back(std::move(muls[i]));
            new_value = std::move(add);
        }
        new_value->try_eval(info);
        side_effect = new_value->side_effect;
        args.clear();
        args.push_back(std::move(new_value));
        return;
    }
    side_effect = false;
    for (auto & arg : args)
    {
        arg->try_eval(info);
        if (arg->side_effect)
            side_effect = true;
    }
    if (start_with(op, "func"))
    {
        side_effect = true;
        return;
    }
    auto& lhs = *args.begin();
    auto& rhs = *args.rbegin();
    if (lhs->value.has_value() && rhs->value.has_value())
    {
        int lhs_val, rhs_val;
        if (try_cal_value_exp(lhs, info).has_value())
            lhs_val = std::stoi(try_cal_value_exp(lhs, info).value());
        else
            return;
        if (try_cal_value_exp(rhs, info).has_value())
            rhs_val = std::stoi(try_cal_value_exp(rhs, info).value());
        else
            return;
        if (op == "add")
            value = std::to_string(lhs_val + rhs_val);
        else if (op == "sub")
            value = std::to_string(lhs_val - rhs_val);
        else if (op == "mul")
            value = std::to_string(lhs_val * rhs_val);
        else if (op == "div")
            value = std::to_string(lhs_val / rhs_val);
        else if (op == "mod")
            value = std::to_string(lhs_val % rhs_val);
        else if (op == "and")
            value = std::to_string(lhs_val && rhs_val);
        else if (op == "or")
            value = std::to_string(lhs_val || rhs_val);
        else if (op == "eq")
            value = std::to_string(lhs_val == rhs_val);
        else if (op == "ne")
            value = std::to_string(lhs_val != rhs_val);
        else if (op == "lt")
            value = std::to_string(lhs_val < rhs_val);
        else if (op == "gt")
            value = std::to_string(lhs_val > rhs_val);
        else if (op == "le")
            value = std::to_string(lhs_val <= rhs_val);
        else if (op == "ge")
            value = std::to_string(lhs_val >= rhs_val);
    }
}

void InitAST::try_eval(std::weak_ptr<IRINFO> info, const List<std::optional<unsigned>> &dims)
{
    if (exp.has_value())
    {
        exp.value()->try_eval(info);
        exps.push_back(std::move(exp.value()));
    }
    else
    {
        int current_num = 0;
        int total_num = 1;
        for (auto &dim : dims)
            total_num *= dim.value();
        for (auto &init : inits)
        {
            if (init->exp.has_value())
                init->try_eval(info, dims);
            else
            {
                int temp = current_num;
                List<std::optional<unsigned>> new_dims = {};
                for (auto it = dims.rbegin(); it != dims.rend(); it++)
                {
                    if (temp % it->value() == 0 && new_dims.size() != dims.size() - 1)
                    {
                        new_dims.push_front(it->value());
                        temp /= it->value();
                    }
                    else
                        break;
                }
                init->try_eval(info, new_dims);
            }
            current_num += init->exps.size();
            exps.merge(init->exps);
        }
        for (int i = 0; i < total_num - current_num; i++)
        {
            auto zero = std::make_unique<ExpAST>();
            zero->value = "0";
            exps.push_back(std::move(zero));
        }
    }
}

std::string InitAST::to_ir_string(std::weak_ptr<IRINFO> info)
{
    std::string result = "{";
    for (auto &exp_ : exps)
    {
        if (exp_->value.has_value() && try_cal_str_value(exp_->value.value(), info))
            result += exp_->value.value() + ", ";
        else
            result += "undef, ";
    }
    result = result.substr(0, result.size() - 2) + "}";
    return result;
}

std::unique_ptr<ExpAST> ExpAST::copy() const
{
    auto result = std::make_unique<ExpAST>();
    result->op = op;
    result->value = value;
    result->arr_name = arr_name;
    result->side_effect = side_effect;
    for (auto & arg : args)
        result->args.push_back(arg->copy());
    return result;
}