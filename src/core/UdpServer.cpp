#include "core/UdpServer.h"
#include "util/Logger.h"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace ezNet {

UdpServer::UdpServer(EventLoop* loop, uint16_t port)
    : loop_(loop), port_(port), sockFd_(-1), running_(false) {
    sockFd_ = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockFd_ < 0) {
        throw std::runtime_error("UdpServer constructor socket failed");
    }
}

UdpServer::~UdpServer() {
    if (sockFd_ >= 0) {
        ::close(sockFd_);
        sockFd_ = -1;
    }
}

void UdpServer::start() {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    if (::bind(sockFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("bind failed");
    }
    loop_->addFd(sockFd_, EPOLLIN, [this](uint32_t){ handleRead(); });
    running_ = true;
    LOG_INFO("UdpServer start on port %d", port_);
}

void UdpServer::stop() {
    loop_->removeFd(sockFd_);
    ::close(sockFd_);
    sockFd_ = -1;
}

void UdpServer::setMessageCallback(MessageCallback cb) {
    messageCallback_ = cb;
}

void UdpServer::sendTo(const char* data, size_t len, const struct sockaddr_in& addr) {
    ::sendto(sockFd_, data, len, 0, (const sockaddr*)&addr, sizeof(addr));
}

EventLoop* UdpServer::loop() const {
    return loop_;
}

uint16_t UdpServer::port() const {
    return port_;
}

void UdpServer::handleRead() {
    while (true) {
        struct sockaddr_in peerAddr;
        socklen_t addrLen = sizeof(peerAddr);
        char buf[65536];
        ssize_t n = recvfrom(sockFd_, buf, sizeof(buf), 0,
                            (struct sockaddr*)&peerAddr, &addrLen);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }
        if (messageCallback_) messageCallback_(buf, n, peerAddr);
    }
}

} // namespace ezNet
