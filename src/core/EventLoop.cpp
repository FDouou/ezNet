#include "core/EventLoop.h"
#include "util/Logger.h"
#include <cstring>
#include <stdexcept>
#include <unistd.h>

namespace ezNet {

EventLoop::EventLoop(TriggerMode mode)
    : epollFd_(epoll_create1(EPOLL_CLOEXEC)), triggerMode_(mode), running_(false), events_(16), fdContexts_(1024) {
    if (epollFd_ < 0) {
        throw std::runtime_error("fail epoll_create1");
    }
}

EventLoop::~EventLoop() {
    close(epollFd_);
}

void EventLoop::addFd(int fd, uint32_t events, EventCallback cb) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (triggerMode_ == TriggerMode::ET) {
        ev.events |= EPOLLET;
    }
    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        throw std::runtime_error("fail epoll_ctl EPOLL_CTL_ADD");
    }

    if (fd >= fdContexts_.size()) {
        fdContexts_.resize(fd + 1);
    }
    fdContexts_[fd] = {fd, events, cb};
}

void EventLoop::modFd(int fd, uint32_t events, EventCallback cb) {
    auto& fdContext = fdContexts_[fd];
    fdContext.events = events;
    fdContext.callback = cb;

    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (triggerMode_ == TriggerMode::ET) {
        ev.events |= EPOLLET;
    }
    if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        throw std::runtime_error("fail epoll_ctl EPOLL_CTL_MOD");
    }
}

void EventLoop::removeFd(int fd) {
    if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        throw std::runtime_error("fail epoll_ctl EPOLL_CTL_DEL");
    }
    fdContexts_[fd].fd = -1;
}

void EventLoop::loop() {
    running_ = true;
    while (running_) {
        int n = epoll_wait(epollFd_, events_.data(), events_.size(), 10000);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < n; ++i) {
            int fd = events_[i].data.fd;
            uint32_t revents = events_[i].events;
            if (fd < 0 || fd >= static_cast<int>(fdContexts_.size())) continue;
            auto cb = fdContexts_[fd].callback;//防止cb扩容造成悬垂
            if (cb) cb(revents);
        }
        if (static_cast<size_t>(n) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    }
}

void EventLoop::stop() {
    running_ = false;
}

} // namespace ezNet
