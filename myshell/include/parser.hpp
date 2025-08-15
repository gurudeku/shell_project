#pragma once
#include <string>
#include <vector>
#include "shell.hpp"

class Parser {
public:
    Pipeline parse(const std::string& line);
private:
    static bool is_meta(char c) { return c=='|' || c=='<' || c=='>' || c=='&'; }
};
