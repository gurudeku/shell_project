#include "util.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>

std::string trim(const std::string& s){
    size_t a=0, b=s.size();
    while(a<b && std::isspace((unsigned char)s[a])) ++a;
    while(b>a && std::isspace((unsigned char)s[b-1])) --b;
    return s.substr(a, b-a);
}

std::vector<std::string> split_ws(const std::string& s){
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while(iss >> tok) out.push_back(tok);
    return out;
}

std::string join(const std::vector<std::string>& v, const std::string& sep){
    std::ostringstream oss;
    for(size_t i=0;i<v.size();++i){
        if(i) oss<<sep;
        oss<<v[i];
    }
    return oss.str();
}
