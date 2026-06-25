#pragma once

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>

namespace ezNet {

class ThreadPool {
public:
    /// @param maxQueueSize 最大队列长度，0 表示无限制
    ThreadPool(size_t numThreads, size_t maxQueueSize = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void enqueue(std::function<void()> task);

    [[nodiscard]] size_t queueSize() const;

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    size_t maxQueueSize_;
};

} // namespace ezNet
