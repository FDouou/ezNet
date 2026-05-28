#pragma once

#include <functional>
#include <memory>
#include <string>
#include "core/Buffer.h"
#include "core/EventLoop.h"

namespace ezNet {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    enum class State {
        ReadingRequest,
        Processing,
        SendingResponse,
        Closing
    };

    using DataCallback = std::function<void(std::shared_ptr<Connection>, Buffer*)>;
    using CloseCallback = std::function<void(std::shared_ptr<Connection>)>;
    using WriteCompleteCallback = std::function<void(std::shared_ptr<Connection>)>;

    Connection(EventLoop* loop, int fd);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    void start();
    void close();
    void forceClose();

    void send(const std::string& data);
    void send(const char* data, size_t len);

    void setDataCallback(DataCallback cb);
    void setCloseCallback(CloseCallback cb);
    const CloseCallback& closeCallback() const;
    void setWriteCompleteCallback(WriteCompleteCallback cb);

    int fd() const;
    State state() const;
    EventLoop* loop() const;
    Buffer& inputBuffer();
    Buffer& outputBuffer();

    bool isKeepAlive() const;
    void setKeepAlive(bool keepAlive);

    void setState(State s);
    void resetForNextRequest();

    void setUserData(void* data) { userData_ = data; }
    void* userData() const { return userData_; }

private:
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    void shutdownWrite();

    EventLoop* loop_;
    int fd_;
    State state_;
    bool keepAlive_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;

    DataCallback dataCallback_;
    CloseCallback closeCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    void* userData_ = nullptr;
};

} // namespace ezNet
