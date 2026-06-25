#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "core/EventLoop.h"
#include "core/Connection.h"

namespace ezNet {

class TcpServer {
public:
    using ConnectionCallback = std::function<void(const std::shared_ptr<Connection>&)>;
    using DataCallback = std::function<void(const std::shared_ptr<Connection>&, Buffer*)>;

    TcpServer(EventLoop* loop, uint16_t port);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void start();
    void stop();

    void setConnectionCallback(ConnectionCallback cb);
    void setDataCallback(DataCallback cb);

    EventLoop* loop() const;
    uint16_t port() const;

private:
    void handleAccept();
    void removeConnection(const std::shared_ptr<Connection>& conn);

    EventLoop* loop_;
    uint16_t port_;
    int listenFd_;
    bool running_;

    ConnectionCallback connectionCallback_;
    DataCallback dataCallback_;

    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
};

} // namespace ezNet
