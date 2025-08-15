#include "completion.hpp"
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#include <vector>
#include <string>
#include <filesystem>

static const char* builtins[] = {"cd","pwd","exit","jobs","fg","bg","kill","history",nullptr};

static char* dupstr(const std::string& s) {
    char* r = (char*)malloc(s.size()+1);
    std::copy(s.begin(), s.end(), r);
    r[s.size()] = '\0';
    return r;
}

static std::vector<std::string> candidates(const std::string& text){
    std::vector<std::string> cand;
    // builtins
    for(int i=0; builtins[i]; ++i){
        std::string b = builtins[i];
        if(b.rfind(text, 0) == 0) cand.push_back(b);
    }
    // files/dirs
    try{
        for(auto& p: std::filesystem::directory_iterator(".")){
            auto name = p.path().filename().string();
            if(name.rfind(text, 0) == 0) cand.push_back(name);
        }
    }catch(...){}
    return cand;
}

static char* generator(const char* text, int state){
    static std::vector<std::string> cand;
    static size_t idx;
    if(state == 0){
        cand = candidates(text);
        idx = 0;
    }
    if(idx < cand.size()){
        return dupstr(cand[idx++]);
    }
    return nullptr;
}

char** myshell_completion(const char* text, int start, int end){
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, generator);
}
#endif
