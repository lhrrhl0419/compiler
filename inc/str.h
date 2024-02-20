#pragma once

#include <string>

bool is_var(const std::string& str);
bool is_tvar(const std::string& str);
bool is_pvar(const std::string& str);
bool is_allocvar(const std::string& str);

bool start_with(const std::string& str1, const std::string& str2);
bool end_with(const std::string& str1, const std::string& str2);
bool is_num(const std::string& str);
void add_tabs(std::string& str, const int tabs);
void new_line(std::string& str, const std::string content="", const int tabs=0);
void start_class(std::string& str, const std::string content, const int tabs);
void end_class(std::string& str, const std::string content, const int tabs);