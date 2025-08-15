#include "logger.hpp"
#include <chrono>
#include <iomanip>

Logger::Logger(const std::string& path): path(path) {
    th = std::thread(&Logger::run, this);
}

Logger::~Logger(){
    stop = true;
    cv.notify_all();
    if(th.joinable()) th.join();
}

void Logger::log(const std::string& line){
    std::unique_lock<std::mutex> lk(mtx);
    q.push(line);
    cv.notify_one();
}

void Logger::run(){
    std::ofstream ofs(path, std::ios::app);
    while(true){
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&]{ return stop || !q.empty(); });
        if(stop && q.empty()) break;
        auto s = q.front(); q.pop();
        lk.unlock();
        // timestamp
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        ofs << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S") << " | " << s << "\n";
        ofs.flush();
    }
}
