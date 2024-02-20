#include <str.h>

bool is_var(const std::string &str)
{
    return str[0] == '%' || str[0] == '@';
}

bool is_tvar(const std::string &str)
{
    return str[0] == '%';
}

bool is_pvar(const std::string &str)
{
    return str[0] == '@';
}

bool is_allocvar(const std::string &str)
{
    return str[0] == '@' || str.substr(0, 6) == std::string("\%alloc");
}

bool start_with(const std::string& str1, const std::string& str2)
{
    return str1.find(str2) == 0;
}

bool end_with(const std::string& str1, const std::string& str2)
{
    return str1.find(str2) == str1.size() - str2.size();
}

bool is_num(const std::string& str)
{
    for (auto c : str)
        if (!isdigit(c) && c != '-')
            return false;
    return true;
}

void add_tabs(std::string& str, const int tabs)
{
    for (int i = 0; i < tabs; i++)
        str += "  ";
}

void new_line(std::string& str, const std::string content, const int tabs)
{
    add_tabs(str, tabs);
    str += content;
    str += "\n";
}

void start_class(std::string& str, const std::string name, const int tabs)
{
    new_line(str, "(" + name + ") {", 0);
}

void end_class(std::string& str, const std::string name, const int tabs)
{
    new_line(str, "}", std::max(0, tabs-1));
}