#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include "core/TimeWheel.h"

namespace ezNet {

using EventCallback = std::function<void(uint32_t events)>;

class EventLoop {
public:
    enum class TriggerMode {
        LT,
        ET
    };

    explicit EventLoop(TriggerMode mode = TriggerMode::ET);
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void addFd(int fd, uint32_t events, EventCallback cb);
    void modFd(int fd, uint32_t events, EventCallback cb);
    void removeFd(int fd);

    void loop();
    void stop();

    /// 跨线程回调：线程安全，任意线程调用，回调会在 EventLoop 线程执行
    void runInLoop(std::function<void()> cb);

    TimeWheel& timeWheel() { return timeWheel_; }

private:
    void executePendingCallbacks();

    int epollFd_;
    TriggerMode triggerMode_;
    std::atomic<bool> running_;
    std::vector<struct epoll_event> events_;

    struct FdContext {
        int fd;
        uint32_t events;
        EventCallback callback;
    };

    std::vector<FdContext> fdContexts_;

    int timerFd_;
    TimeWheel timeWheel_;

    // runInLoop 相关
    std::queue<std::function<void()>> pendingCallbacks_;
    mutable std::mutex pendingMutex_;
};

} // namespace ezNet
