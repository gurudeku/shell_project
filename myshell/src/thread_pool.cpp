#include "thread_pool.hpp"

ThreadPool::ThreadPool(size_t n){
    if(n==0) n=1;
    for(size_t i=0;i<n;++i){
        workers.emplace_back([this]{
            while(true){
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lk(mtx);
                    cv.wait(lk, [&]{ return stop || !tasks.empty(); });
                    if(stop && tasks.empty()) return;
                    task = std::move(tasks.front()); tasks.pop();
                }
                task();
            }
        });
    }
}
ThreadPool::~ThreadPool(){
    stop = true;
    cv.notify_all();
    for(auto& t: workers) if(t.joinable()) t.join();
}
void ThreadPool::enqueue(std::function<void()> f){
    {
        std::lock_guard<std::mutex> lk(mtx);
        tasks.push(std::move(f));
    }
    cv.notify_one();
}
