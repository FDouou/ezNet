#include "core/Timer.h"
#include "util/Logger.h"
#include <cstring>
#include <stdexcept>
#include <sys/timerfd.h>
#include <unistd.h>

namespace ezNet {

Timer::Timer(EventLoop* loop)
    : loop_(loop), timerFd_(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)), running_(false), repeat_(false) {
    if (timerFd_ == -1) {
        throw std::runtime_error("timerfd_create failed");
    }
}

Timer::~Timer() {
    stop();
    close(timerFd_);
}

void Timer::start(double intervalSec, bool repeat, TimerCallback cb) {
    repeat_ = repeat;
    callback_ = cb;
    struct itimerspec ts;
    ts.it_value.tv_sec = (time_t)intervalSec;
    ts.it_value.tv_nsec = (long)((intervalSec - (time_t)intervalSec) * 1e9);
    ts.it_interval.tv_sec = repeat ? ts.it_value.tv_sec : 0;
    ts.it_interval.tv_nsec = repeat ? ts.it_value.tv_nsec : 0;
    timerfd_settime(timerFd_, 0, &ts, nullptr);
    loop_->addFd(timerFd_, EPOLLIN, [this](uint32_t){ handleTimeout(); });
    running_ = true;
}

void Timer::stop() {
    if (!running_) return;
    loop_->removeFd(timerFd_);
    struct itimerspec ts = {};
    timerfd_settime(timerFd_, 0, &ts, nullptr);
    running_ = false;
}

bool Timer::isRunning() const {
    return running_;
}

void Timer::handleTimeout() {
    uint64_t exp;
    ::read(timerFd_, &exp, sizeof(exp));
    callback_();
    if(!repeat_) {
        stop();
    }
}

} // namespace ezNet
