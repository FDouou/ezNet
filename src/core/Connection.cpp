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

void Connection::sendFile(int fileFd, size_t fileSize, off_t offset, size_t length) {
    if (sendingFile_) {
        LOG_ERROR("sendFile: already sending a file, ignoring fd=%d", fileFd);
        ::close(fileFd);  // 关闭调用方传入的 fd，避免泄漏
        return;
    }

    // 如果 offset > 0，定位到指定位置
    if (offset > 0) {
        off_t result = ::lseek(fileFd, offset, SEEK_SET);
        if (result == (off_t)-1) {
            ::close(fileFd);
            LOG_ERROR("sendFile: lseek failed for fd=%d", fileFd);
            return;
        }
    }

    sendingFile_ = true;
    fileFd_ = fileFd;  // 接管 fd 所有权（完成后在 handleWrite 里 close）
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
    // ET 模式循环读取直到 EAGAIN
    auto self = shared_from_this();  // 提前获取，循环内复用避免多次原子操作
    while (true) {
        int savedErrno = 0;
        ssize_t n = inputBuffer_.readFromFd(fd_, &savedErrno);
        if (n > 0) {
            // 成功读取数据，调用回调
            if (dataCallback_) {
                dataCallback_(self, &inputBuffer_);
            }
            continue;
        }
        if (n == 0) {
            // EOF：对端关闭连接
            handleClose();
            return;
        }
        // n < 0
        if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK) {
            // 数据已读完，正常返回
            return;
        }
        // 真实错误
        handleError();
        return;
    }
}

void Connection::handleWrite() {
    auto self = shared_from_this();  // 提前获取，避免多次原子操作
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
                writeCompleteCallback_(self);
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
            if (savedErrno != EAGAIN && savedErrno != EWOULDBLOCK) {
                handleError();
            }
            return;  // EAGAIN：等下一轮 EPOLLOUT
        }
        if (n == 0) break;  // 写了 0 字节（不太可能）
    }
    if (outputBuffer_.readableBytes() == 0) {
        loop_->modFd(fd_, EPOLLIN, [this](uint32_t events) {
            if (events & EPOLLIN) handleRead();
            if (events & (EPOLLERR | EPOLLHUP)) handleError();
        });
        if (writeCompleteCallback_) {
            writeCompleteCallback_(self);
        }
        if (state_ == State::Closing) {
            handleClose();
        }
    }
}

void Connection::handleClose() {
    if (fd_ < 0) return;

    // 如果正在发送文件，通知 writeCompleteCallback（fd 还有效，上层需要 fd 清理资源）
    // 这样异常断开时也能触发下载计数清理，避免计数器泄漏
    if (sendingFile_ && writeCompleteCallback_) {
        auto self = shared_from_this();
        writeCompleteCallback_(self);
    }

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
