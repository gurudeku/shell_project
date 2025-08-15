#pragma once
#include <string>
#include <vector>

class History {
public:
    History();
    void add(const std::string& line);
    void load();
    void save();
    void print() const;
    const std::vector<std::string>& data() const { return lines; }
private:
    std::string path;
    std::vector<std::string> lines;
};
