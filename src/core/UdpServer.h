#pragma once

#include <functional>
#include <string>
#include <netinet/in.h>
#include "core/Buffer.h"
#include "core/EventLoop.h"

namespace ezNet {

class UdpServer {
public:
    using MessageCallback = std::function<void(const char* data, size_t len,
                                               const struct sockaddr_in& addr)>;

    UdpServer(EventLoop* loop, uint16_t port);
    ~UdpServer();

    UdpServer(const UdpServer&) = delete;
    UdpServer& operator=(const UdpServer&) = delete;

    void start();
    void stop();

    void setMessageCallback(MessageCallback cb);

    void sendTo(const char* data, size_t len, const struct sockaddr_in& addr);

    EventLoop* loop() const;
    uint16_t port() const;

private:
    void handleRead();

    EventLoop* loop_;
    uint16_t port_;
    int sockFd_;
    bool running_;

    Buffer readBuffer_;
    MessageCallback messageCallback_;
};

} // namespace ezNet
