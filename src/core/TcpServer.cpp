#include "core/TcpServer.h"
#include "util/Logger.h"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace ezNet {

static int createNonBlockingSocket() {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(fd < 0) {
        throw std::runtime_error("create socket failed");
    }
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    return fd;
}

TcpServer::TcpServer(EventLoop* loop, uint16_t port)
    : loop_(loop), port_(port), listenFd_(createNonBlockingSocket()), running_(false) {

}

TcpServer::~TcpServer() {
    if (listenFd_ >= 0) {
        ::close(listenFd_);
        listenFd_ = -1;
    }
    for (auto& kv : connections_) {
        kv.second->setCloseCallback(nullptr);
    }
    connections_.clear();
}

void TcpServer::start() {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    if (::bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("bind failed");
    }
    if (::listen(listenFd_, SOMAXCONN) < 0) {
        throw std::runtime_error("listen failed");
    }
    loop_->addFd(listenFd_, EPOLLIN, [this](uint32_t){ handleAccept(); });
    running_ = true;
    LOG_INFO("TcpServer start on port %d", port_);
}

void TcpServer::stop() {
    loop_->removeFd(listenFd_);
    ::close(listenFd_);
    listenFd_ = -1;
    std::unordered_map<int, std::shared_ptr<Connection>> conns;
    conns.swap(connections_);
    for (auto& kv : conns) {
        kv.second->forceClose();
    }
}

void TcpServer::setConnectionCallback(ConnectionCallback cb) {
    connectionCallback_ = cb;
}

void TcpServer::setDataCallback(DataCallback cb) {
    dataCallback_ = cb;
}

EventLoop* TcpServer::loop() const {
    return loop_;
}

uint16_t TcpServer::port() const {
    return port_;
}

void TcpServer::handleAccept() {
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int connFd = accept4(listenFd_, (struct sockaddr*)&clientAddr,
                            &addrLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // 全部 accept 完毕
            // 其他错误，视情况 continue 或 break
            break;
        }
        auto conn = std::make_shared<Connection>(loop_, connFd);
        connections_[connFd] = conn;
        conn->setDataCallback(dataCallback_);
        conn->setCloseCallback([this](const std::shared_ptr<Connection>& c) {
            removeConnection(c);
        });
        conn->start();

        std::weak_ptr<Connection> weakConn = conn;
        auto* entry = loop_->timeWheel().add(60.0, [weakConn]() {
            if (auto c = weakConn.lock()) {
                c->setWheelEntry(nullptr);
                c->forceClose();
            }
        }, weakConn);
        conn->setWheelEntry(entry);

        if (connectionCallback_) connectionCallback_(conn);
    }
}

void TcpServer::removeConnection(const std::shared_ptr<Connection>& conn) {
    if (conn->wheelEntry()) {
        loop_->timeWheel().remove(static_cast<TimeWheel::Entry*>(conn->wheelEntry()));
        conn->setWheelEntry(nullptr);
    }
    connections_.erase(conn->fd());
}

} // namespace ezNet
