#include "parser.hpp"
#include "util.hpp"
#include <sstream>

static void push_arg(Command& cmd, std::string& buf){
    if(!buf.empty()){
        cmd.argv.push_back(buf);
        buf.clear();
    }
}

Pipeline Parser::parse(const std::string& line){
    Pipeline pl;
    Command cur;
    std::string buf;
    bool in_squote=false, in_dquote=false;
    for(size_t i=0;i<line.size();++i){
        char c=line[i];
        if(c=='\\'){ // escape
            if(i+1<line.size()) { buf.push_back(line[++i]); }
            else buf.push_back('\\');
            continue;
        }
        if(c=='\'' && !in_dquote){ in_squote = !in_squote; continue; }
        if(c=='\"' && !in_squote){ in_dquote = !in_dquote; continue; }
        if(!in_squote && !in_dquote){
            if(std::isspace((unsigned char)c)){
                push_arg(cur, buf);
                continue;
            }
            if(c=='|'){
                push_arg(cur, buf);
                if(!cur.argv.empty()) pl.cmds.push_back(cur);
                cur = Command{};
                continue;
            }
            if(c=='<'){
                push_arg(cur, buf);
                // read filename
                std::string fname;
                size_t j=i+1;
                while(j<line.size() && std::isspace((unsigned char)line[j])) ++j;
                while(j<line.size() && !std::isspace((unsigned char)line[j]) && line[j] != '|' && line[j] != '&') fname.push_back(line[j++]);
                cur.in = fname;
                i = j-1;
                continue;
            }
            if(c=='>'){
                push_arg(cur, buf);
                bool append=false;
                size_t j=i+1;
                if(j<line.size() && line[j]=='>'){ append=true; ++j; }
                while(j<line.size() && std::isspace((unsigned char)line[j])) ++j;
                std::string fname;
                while(j<line.size() && !std::isspace((unsigned char)line[j]) && line[j] != '|' && line[j] != '&') fname.push_back(line[j++]);
                cur.out = fname;
                cur.append_out = append;
                i=j-1;
                continue;
            }
            if(c=='&'){
                push_arg(cur, buf);
                pl.background = true;
                continue;
            }
        }
        buf.push_back(c);
    }
    push_arg(cur, buf);
    if(!cur.argv.empty()) pl.cmds.push_back(cur);
    return pl;
}
