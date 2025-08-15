#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>

class Logger {
public:
    explicit Logger(const std::string& path);
    ~Logger();
    void log(const std::string& line);
private:
    void run();
    std::string path;
    std::thread th;
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<std::string> q;
    std::atomic<bool> stop{false};
};
