#include "core/EventLoop.h"
#include "core/UdpServer.h"
#include "util/Logger.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>
#include <atomic>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace ezNet;

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; g_fail++; } \
    else { g_pass++; } \
} while(0)

static int createUdpClient(uint16_t serverPort, uint16_t* localPort) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;
    if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }

    if (localPort) {
        socklen_t len = sizeof(addr);
        getsockname(fd, (struct sockaddr*)&addr, &len);
        *localPort = ntohs(addr.sin_port);
    }
    return fd;
}

void test_construct_destruct() {
    EventLoop loop;
    UdpServer server(&loop, 19980);
    EXPECT(true, "UdpServer construct/destruct");
}

void test_start_and_stop() {
    EventLoop loop;
    UdpServer server(&loop, 19981);
    server.start();
    EXPECT(server.loop() == &loop, "loop() returns correct pointer");
    EXPECT(server.port() == 19981, "port() returns correct port");
    server.stop();
    EXPECT(true, "UdpServer start/stop cleanly");
}

void test_echo_send_receive() {
    EventLoop loop;
    UdpServer server(&loop, 19982);

    std::atomic<bool> received{false};
    std::string receivedData;
    struct sockaddr_in fromAddr;
    memset(&fromAddr, 0, sizeof(fromAddr));

    server.setMessageCallback([&](const char* data, size_t len,
                                   const struct sockaddr_in& addr) {
        received = true;
        receivedData.assign(data, len);
        fromAddr = addr;
        server.sendTo(data, len, addr);
    });

    server.start();
    std::thread t([&]() { loop.loop(); });

    int clientFd = createUdpClient(19982, nullptr);
    EXPECT(clientFd >= 0, "UDP client socket created");

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(19982);

    const char* msg = "Hello ezNet UDP!";
    ssize_t sent = ::sendto(clientFd, msg, strlen(msg), 0,
                            (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    EXPECT(sent == (ssize_t)strlen(msg), "UDP client sent data");

    char recvBuf[256];
    struct timeval tv = {2, 0};
    setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ssize_t n = ::recvfrom(clientFd, recvBuf, sizeof(recvBuf) - 1, 0, nullptr, nullptr);

    EXPECT(received.load(), "UDP server received message");
    EXPECT(receivedData == msg, "server received data matches sent data");
    EXPECT(n == sent, "UDP client received echo response");
    if (n > 0) {
        recvBuf[n] = '\0';
        EXPECT(strcmp(recvBuf, msg) == 0, "echo content matches original");
    }

    loop.stop();
    t.join();
    ::close(clientFd);
}

void test_multiple_messages() {
    EventLoop loop;
    UdpServer server(&loop, 19983);

    std::atomic<int> msgCount{0};
    server.setMessageCallback([&](const char* data, size_t len,
                                   const struct sockaddr_in& addr) {
        msgCount++;
        server.sendTo(data, len, addr);
    });

    server.start();
    std::thread t([&]() { loop.loop(); });

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    saddr.sin_port = htons(19983);

    int clientFd = createUdpClient(19983, nullptr);
    for (int i = 0; i < 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "msg%d", i);
        ::sendto(clientFd, buf, strlen(buf), 0,
                 (struct sockaddr*)&saddr, sizeof(saddr));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT(msgCount.load() == 5, "UDP server received all 5 messages");

    loop.stop();
    t.join();
    ::close(clientFd);
}

void test_different_clients() {
    EventLoop loop;
    UdpServer server(&loop, 19984);

    std::atomic<int> msgCount{0};
    struct sockaddr_in lastAddr;
    memset(&lastAddr, 0, sizeof(lastAddr));

    server.setMessageCallback([&](const char* data, size_t len,
                                   const struct sockaddr_in& addr) {
        msgCount++;
        lastAddr = addr;
        server.sendTo(data, len, addr);
    });

    server.start();
    std::thread t([&]() { loop.loop(); });

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    saddr.sin_port = htons(19984);

    uint16_t p1, p2;
    int c1 = createUdpClient(19984, &p1);
    int c2 = createUdpClient(19984, &p2);
    EXPECT(p1 != p2, "two clients have different local ports");

    ::sendto(c1, "from_c1", 7, 0, (struct sockaddr*)&saddr, sizeof(saddr));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(msgCount.load() == 1, "message from client 1 received");

    ::sendto(c2, "from_c2", 7, 0, (struct sockaddr*)&saddr, sizeof(saddr));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(msgCount.load() == 2, "message from client 2 received");

    loop.stop();
    t.join();
    ::close(c1); ::close(c2);
}

int main() {
    std::cout << "=== UdpServer Tests ===" << std::endl;

    test_construct_destruct();
    test_start_and_stop();
    test_echo_send_receive();
    test_multiple_messages();
    test_different_clients();

    std::cout << "Passed: " << g_pass << ", Failed: " << g_fail << std::endl;
    return g_fail > 0 ? 1 : 0;
}
