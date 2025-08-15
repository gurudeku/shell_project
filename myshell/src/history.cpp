#include "history.hpp"
#include <iostream>
#include <cstdlib>
#include <fstream>

History::History(){
    const char* home = std::getenv("HOME");
    if(!home) home = ".";
    path = std::string(home) + "/.myshell_history";
}

void History::add(const std::string& line){
    if(line.empty()) return;
    lines.push_back(line);
}

void History::load(){
    std::ifstream ifs(path);
    std::string s;
    while(std::getline(ifs, s)){
        if(!s.empty()) lines.push_back(s);
    }
}

void History::save(){
    std::ofstream ofs(path, std::ios::app);
    for(const auto& s: lines) ofs << s << "\n";
    lines.clear();
}

void History::print() const{
    for(size_t i=0;i<lines.size();++i){
        std::cout << (i+1) << "  " << lines[i] << "\n";
    }
}
