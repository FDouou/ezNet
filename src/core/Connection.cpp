#include "core/Connection.h"
#include "util/Logger.h"
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ezNet {

Connection::Connection(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), state_(State::ReadingRequest), keepAlive_(false) {
    //保留原有label并设置非阻塞
    fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);
    int opt = 1;
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    setState(State::ReadingRequest);
}

Connection::~Connection() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void Connection::start() {
    loop_->addFd(fd_, EPOLLIN, [this](uint32_t events) {
        if (events & EPOLLIN) handleRead();
        if (events & EPOLLOUT) handleWrite();
        if (events & (EPOLLERR | EPOLLHUP)) handleError();
    });
}

void Connection::close() {
    setState(State::Closing);
    if (outputBuffer_.readableBytes() == 0) {
        forceClose();
    } else {
        
    }
}

void Connection::forceClose() {
    shutdownWrite();
    handleClose();
}

void Connection::send(const std::string& data) {
    send(data.c_str(), data.size());
}

void Connection::send(const char* data, size_t len) {
    //先直接往内核写，写不完就写入outputBuffer，并设置监听EPOLLOUT事件
    if (outputBuffer_.readableBytes() == 0) {
        ssize_t n = ::write(fd_, data, len);
        if (n > 0) {
            data += n;
            len -= n;
        }
        if (n < 0 && errno != EAGAIN) {
            handleError();
            return;
        }
        outputBuffer_.append(data, len);
    } else {
        outputBuffer_.append(data, len);
    }

    if (outputBuffer_.readableBytes() > 0) {
        loop_->modFd(fd_, EPOLLIN | EPOLLOUT, [this](uint32_t events) {
            if (events & EPOLLIN) handleRead();
            if (events & EPOLLOUT) handleWrite();
            if (events & (EPOLLERR | EPOLLHUP)) handleError();
        });
    }
}

void Connection::setDataCallback(DataCallback cb) {
    dataCallback_ = cb;
}

void Connection::setCloseCallback(CloseCallback cb) {
    closeCallback_ = cb;
}

const Connection::CloseCallback& Connection::closeCallback() const {
    return closeCallback_;
}

void Connection::setWriteCompleteCallback(WriteCompleteCallback cb) {
    writeCompleteCallback_ = cb;
}

int Connection::fd() const {
    return fd_;
}

Connection::State Connection::state() const {
    return state_;
}

EventLoop* Connection::loop() const {
    return loop_;
}

Buffer& Connection::inputBuffer() {
    return inputBuffer_;
}

Buffer& Connection::outputBuffer() {
    return outputBuffer_;
}

bool Connection::isKeepAlive() const {
    return keepAlive_;
}

void Connection::setKeepAlive(bool keepAlive) {
    keepAlive_ = keepAlive;
}

void Connection::setState(State s) {
    state_ = s;
}

void Connection::resetForNextRequest() {
    inputBuffer_.retrieveAll();
    setState(State::ReadingRequest);
}

void Connection::handleRead() {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFromFd(fd_, &savedErrno);
    if (n < 0 && errno != EAGAIN) {
        handleError();
        return;
    }
    if (n == 0) {
        handleClose();
        return;
    }
    if (dataCallback_) {
        dataCallback_(shared_from_this(), &inputBuffer_);
    }
}

void Connection::handleWrite() {
    while (outputBuffer_.readableBytes() > 0) {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeToFd(fd_, &savedErrno);
        if (n < 0) {
            handleError();
            return;
        }
        if (n == 0) break;
    }
    if (outputBuffer_.readableBytes() == 0) {
        loop_->modFd(fd_, EPOLLIN, [this](uint32_t events) {
            if (events & EPOLLIN) handleRead();
            if (events & (EPOLLERR | EPOLLHUP)) handleError();
        });
        if (writeCompleteCallback_) {
            writeCompleteCallback_(shared_from_this());
        }
        if (state_ == State::Closing) {
            handleClose();
        }
    }
}

void Connection::handleClose() {
    if (fd_ < 0) return;
    loop_->removeFd(fd_);
    ::close(fd_);
    fd_ = -1;
    auto cb = std::move(closeCallback_);
    if (cb) cb(shared_from_this());
}

void Connection::handleError() {
    handleClose();
}

void Connection::shutdownWrite() {
    ::shutdown(fd_, SHUT_WR);
}

} // namespace ezNet
