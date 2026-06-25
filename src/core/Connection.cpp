#include "core/Connection.h"
#include "util/Logger.h"
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
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
    // 检查连接状态，仅在有效状态下允许发送
    if (state_ != State::ReadingRequest &&
        state_ != State::Processing &&
        state_ != State::SendingResponse) {
        return;
    }

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

void Connection::sendFile(const std::string& filePath, size_t fileSize, off_t offset, size_t length) {
    if (sendingFile_) {
        LOG_ERROR("sendFile: already sending a file, ignoring %s", filePath.c_str());
        return;
    }

    int fd = ::open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("sendFile: cannot open %s", filePath.c_str());
        return;
    }

    // 如果 offset > 0，定位到指定位置
    if (offset > 0) {
        off_t result = ::lseek(fd, offset, SEEK_SET);
        if (result == (off_t)-1) {
            ::close(fd);
            LOG_ERROR("sendFile: lseek failed for %s", filePath.c_str());
            return;
        }
    }

    sendingFile_ = true;
    fileFd_ = fd;
    fileSize_ = fileSize;
    fileOffset_ = offset;
    bytesToSend_ = (length == 0) ? (fileSize - static_cast<size_t>(offset)) : length;
    fileSentSize_ = 0;

    // 注册 EPOLLOUT，等待可写后发送文件
    auto self = shared_from_this();
    loop_->modFd(fd_, EPOLLIN | EPOLLOUT, [self](uint32_t events) {
        if (events & EPOLLIN) self->handleRead();
        if (events & EPOLLOUT) self->handleWrite();
        if (events & (EPOLLERR | EPOLLHUP)) self->handleError();
    });
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
    if (n < 0) {
        if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK) return;
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
    // 文件发送模式（sendfile）
    if (sendingFile_) {
        // 先发送 outputBuffer 中的 HTTP 头部
        if (outputBuffer_.readableBytes() > 0) {
            int savedErrno = 0;
            ssize_t n = outputBuffer_.writeToFd(fd_, &savedErrno);
            if (n < 0) {
                if (savedErrno != EAGAIN && savedErrno != EWOULDBLOCK) {
                    handleError();
                }
                return;
            }
        }

        if (outputBuffer_.readableBytes() > 0) {
            // 头部还没发完，等下一轮 EPOLLOUT
            return;
        }

        // 头部已发完，开始 sendfile
        if (fileFd_ < 0) {
            handleError();
            return;
        }

        while (fileSentSize_ < bytesToSend_) {
            off_t offset = static_cast<off_t>(fileOffset_ + fileSentSize_);
            size_t remaining = bytesToSend_ - fileSentSize_;
            ssize_t n = ::sendfile(fd_, fileFd_, &offset, remaining);
            if (n > 0) {
                fileSentSize_ += n;
            } else if (n == 0) {
                // 文件读完
                break;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 内核缓冲区满，等下一轮 EPOLLOUT
                    return;
                }
                if (errno == EINTR) {
                    // 被信号中断，重试
                    continue;
                }
                handleError();
                return;
            }
        }

        if (fileSentSize_ >= bytesToSend_) {
            // 文件发送完毕
            ::close(fileFd_);
            fileFd_ = -1;
            sendingFile_ = false;

            // 去掉 EPOLLOUT
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
        return;
    }

    // 普通 buffer 发送模式（原逻辑）
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
    if (fileFd_ >= 0) {
        ::close(fileFd_);
        fileFd_ = -1;
        sendingFile_ = false;
    }
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
