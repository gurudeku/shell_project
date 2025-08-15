#pragma once
#include <string>
#include <vector>

std::string trim(const std::string& s);
std::vector<std::string> split_ws(const std::string& s);
std::string join(const std::vector<std::string>& v, const std::string& sep);
