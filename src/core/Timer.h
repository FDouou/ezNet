#pragma once

#include <functional>
#include <sys/timerfd.h>
#include "core/EventLoop.h"

namespace ezNet {

class Timer {
public:
    using TimerCallback = std::function<void()>;

    Timer(EventLoop* loop);
    ~Timer();

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    void start(double intervalSec, bool repeat, TimerCallback cb);
    void stop();

    bool isRunning() const;

private:
    void handleTimeout();

    EventLoop* loop_;
    int timerFd_;
    bool running_;
    bool repeat_;
    TimerCallback callback_;
};

} // namespace ezNet
