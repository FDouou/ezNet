#include "core/EventLoop.h"
#include "core/TcpServer.h"
#include "core/Connection.h"
#include "core/Buffer.h"
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

static int connectTo(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);
    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

static ssize_t readAll(int fd, char* buf, size_t maxLen, int timeoutMs) {
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return ::recv(fd, buf, maxLen, 0);
}

void test_construct_destruct() {
    EventLoop loop;
    TcpServer server(&loop, 19990);
    EXPECT(true, "TcpServer construct/destruct");
}

void test_start_and_stop() {
    EventLoop loop;
    TcpServer server(&loop, 19991);
    server.start();
    EXPECT(server.loop() == &loop, "loop() returns correct pointer");
    EXPECT(server.port() == 19991, "port() returns correct port");
    server.stop();
    EXPECT(true, "TcpServer start/stop cleanly");
}

void test_accept_and_connection_callback() {
    EventLoop loop;
    TcpServer server(&loop, 19992);

    std::atomic<bool> cbCalled{false};
    server.setConnectionCallback([&](const std::shared_ptr<Connection>& conn) {
        cbCalled = true;
        EXPECT(conn->fd() > 0, "accepted connection has valid fd");
        EXPECT(conn->state() == Connection::State::ReadingRequest,
               "accepted connection in ReadingRequest state");
    });

    server.start();

    std::thread t([&]() { loop.loop(); });

    int clientFd = connectTo(19992);
    EXPECT(clientFd >= 0, "client connected to TcpServer");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(cbCalled.load(), "connection callback was fired");

    loop.stop();
    t.join();
    ::close(clientFd);
}

void test_client_send_data_and_server_receive() {
    EventLoop loop;
    TcpServer server(&loop, 19993);

    std::atomic<bool> dataReceived{false};
    std::string receivedData;
    std::shared_ptr<Connection> serverConn;

    server.setDataCallback([&](const std::shared_ptr<Connection>& conn, Buffer* buf) {
        dataReceived = true;
        serverConn = conn;
        receivedData = std::string(buf->peek(), buf->readableBytes());
        buf->retrieveAll();
    });

    server.start();
    std::thread t([&]() { loop.loop(); });

    int clientFd = connectTo(19993);
    EXPECT(clientFd >= 0, "client connected");

    const char* msg = "Hello ezNet TCP!";
    ssize_t sent = ::send(clientFd, msg, strlen(msg), 0);
    EXPECT(sent == (ssize_t)strlen(msg), "client sent data");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT(dataReceived.load(), "data callback fired");
    EXPECT(receivedData == msg, "received data matches sent data");

    loop.stop();
    t.join();
    ::close(clientFd);
}

void test_server_send_and_client_receive() {
    EventLoop loop;
    TcpServer server(&loop, 19994);

    server.setDataCallback([&](const std::shared_ptr<Connection>& conn, Buffer* buf) {
        buf->retrieveAll();
        conn->send("ECHO: hello");
        conn->close();
    });

    server.start();
    std::thread t([&]() { loop.loop(); });

    int clientFd = connectTo(19994);
    EXPECT(clientFd >= 0, "client connected");

    ::send(clientFd, "ping", 4, 0);
    char buf[256];
    ssize_t n = readAll(clientFd, buf, sizeof(buf) - 1, 3000);
    EXPECT(n > 0, "client received response");
    if (n > 0) {
        buf[n] = '\0';
        EXPECT(strcmp(buf, "ECHO: hello") == 0, "response content correct");
    }

    loop.stop();
    t.join();
    ::close(clientFd);
}

void test_multiple_clients() {
    EventLoop loop;
    TcpServer server(&loop, 19995);

    std::atomic<int> connCount{0};
    std::atomic<int> dataCount{0};

    server.setConnectionCallback([&](const std::shared_ptr<Connection>&) {
        connCount++;
    });
    server.setDataCallback([&](const std::shared_ptr<Connection>&, Buffer* buf) {
        dataCount++;
        buf->retrieveAll();
    });

    server.start();
    std::thread t([&]() { loop.loop(); });

    int c1 = connectTo(19995);
    int c2 = connectTo(19995);
    int c3 = connectTo(19995);
    EXPECT(c1 >= 0 && c2 >= 0 && c3 >= 0, "3 clients connected");

    ::send(c1, "a", 1, 0);
    ::send(c2, "b", 1, 0);
    ::send(c3, "c", 1, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT(connCount.load() >= 3, "connection callback fired for 3 clients");
    EXPECT(dataCount.load() >= 3, "data callback fired for 3 clients");

    loop.stop();
    t.join();
    ::close(c1); ::close(c2); ::close(c3);
}

void test_connection_close_callback() {
    EventLoop loop;
    TcpServer server(&loop, 19996);

    std::atomic<bool> closeCalled{false};
    server.setDataCallback([&](const std::shared_ptr<Connection>& conn, Buffer* buf) {
        buf->retrieveAll();
        conn->close();
    });
    server.setConnectionCallback([&](const std::shared_ptr<Connection>& conn) {
        conn->setCloseCallback([&](const std::shared_ptr<Connection>&) {
            closeCalled = true;
        });
    });

    server.start();
    std::thread t([&]() { loop.loop(); });

    int clientFd = connectTo(19996);
    ::send(clientFd, "close me", 8, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT(closeCalled.load(), "close callback fired when connection closes");

    loop.stop();
    t.join();
    ::close(clientFd);
}

int main() {
    std::cout << "=== TcpServer + Connection Tests ===" << std::endl;

    test_construct_destruct();
    test_start_and_stop();
    test_accept_and_connection_callback();
    test_client_send_data_and_server_receive();
    test_server_send_and_client_receive();
    test_multiple_clients();
    test_connection_close_callback();

    std::cout << "Passed: " << g_pass << ", Failed: " << g_fail << std::endl;
    return g_fail > 0 ? 1 : 0;
}
