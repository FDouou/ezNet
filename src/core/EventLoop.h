#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include <functional>
#include <memory>
#include <vector>
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

    TimeWheel& timeWheel() { return timeWheel_; }

private:
    int epollFd_;
    TriggerMode triggerMode_;
    bool running_;
    std::vector<struct epoll_event> events_;

    struct FdContext {
        int fd;
        uint32_t events;
        EventCallback callback;
    };

    std::vector<FdContext> fdContexts_;

    int timerFd_;
    TimeWheel timeWheel_;
};

} // namespace ezNet
