#include "util/ThreadPool.h"
#include <iostream>
#include <stdexcept>

namespace ezNet {

ThreadPool::ThreadPool(size_t numThreads, size_t maxQueueSize)
    : stop_(false), maxQueueSize_(maxQueueSize) {
    workers_.reserve(numThreads);
    size_t i = 0;
    try {
        for (; i < numThreads; ++i) {
            workers_.emplace_back(&ThreadPool::workerLoop, this);
        }
    } catch (...) {
        // 线程创建失败：通知所有已创建的线程退出，join 它们，然后重新抛出
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        workers_.clear();
        throw;
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (maxQueueSize_ > 0 && tasks_.size() >= maxQueueSize_) {
            throw std::runtime_error("ThreadPool queue full");
        }
        
        tasks_.emplace(std::move(task));
    }
    cv_.notify_one();
}

size_t ThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "[ThreadPool] Unhandled exception in worker: "
                      << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[ThreadPool] Unknown unhandled exception in worker"
                      << std::endl;
        }
    }
}

} // namespace ezNet
