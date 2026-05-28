#include "core/EventLoop.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <unistd.h>

using namespace ezNet;

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; g_fail++; } \
    else { g_pass++; } \
} while(0)

// LT

void test_construct_destruct() {
    EventLoop loop(EventLoop::TriggerMode::LT);
    EXPECT(true, "construct LT EventLoop");
}

void test_addFd_and_EPOLLIN() {
    EventLoop loop(EventLoop::TriggerMode::LT);

    int fds[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    EXPECT(ret == 0, "socketpair create");

    std::atomic<bool> called{false};
    std::atomic<uint32_t> gotEvents{0};

    loop.addFd(fds[0], EPOLLIN, [&](uint32_t events) {
        called = true;
        gotEvents = events;
    });

    std::thread t([&]() { loop.loop(); });

    const char* msg = "hello";
    ssize_t n = write(fds[1], msg, 5);
    EXPECT(n == 5, "write to pair fd");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT(called.load(), "EPOLLIN callback fired");
    EXPECT((gotEvents.load() & EPOLLIN) != 0, "events contains EPOLLIN");

    loop.stop();
    t.join();
    close(fds[0]);
    close(fds[1]);
}

void test_modFd() {
    EventLoop loop(EventLoop::TriggerMode::LT);

    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    std::atomic<bool> readFired{false};
    std::atomic<bool> writeFired{false};

    loop.addFd(fds[0], EPOLLIN, [&](uint32_t) {
        readFired = true;
    });

    loop.modFd(fds[0], EPOLLOUT, [&](uint32_t) {
        writeFired = true;
    });

    std::thread t([&]() { loop.loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT(!readFired.load(), "modFd: old EPOLLIN callback NOT fired (mod replaced it)");
    EXPECT(writeFired.load(), "modFd: new EPOLLOUT callback fired");

    loop.stop();
    t.join();
    close(fds[0]);
    close(fds[1]);
}

void test_removeFd() {
    EventLoop loop(EventLoop::TriggerMode::LT);

    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    std::atomic<bool> called{false};

    loop.addFd(fds[0], EPOLLIN, [&](uint32_t) { called = true; });
    loop.removeFd(fds[0]);

    std::thread t([&]() { loop.loop(); });

    write(fds[1], "x", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT(!called.load(), "removeFd: callback NOT fired after removal");

    loop.stop();
    t.join();
    close(fds[0]);
    close(fds[1]);
}

void test_stop_from_inside_callback() {
    EventLoop loop(EventLoop::TriggerMode::LT);

    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    loop.addFd(fds[0], EPOLLIN, [&](uint32_t) {
        loop.stop();
    });

    std::thread t([&]() { loop.loop(); });

    write(fds[1], "x", 1);
    t.join();

    EXPECT(true, "stop() from inside callback exits cleanly");
    close(fds[0]);
    close(fds[1]);
}

void test_multiple_events() {
    EventLoop loop(EventLoop::TriggerMode::LT);

    int fds1[2], fds2[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds1);
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds2);

    std::atomic<int> count{0};

    auto cb = [&](uint32_t) { count++; };

    loop.addFd(fds1[0], EPOLLIN, cb);
    loop.addFd(fds2[0], EPOLLIN, cb);

    std::thread t([&]() { loop.loop(); });

    write(fds1[1], "a", 1);
    write(fds2[1], "b", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT(count.load() >= 2, "multiple fds: both callbacks fired");

    loop.stop();
    t.join();
    close(fds1[0]); close(fds1[1]);
    close(fds2[0]); close(fds2[1]);
}

void test_read_data_in_callback() {
    EventLoop loop(EventLoop::TriggerMode::LT);

    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    std::string received;

    loop.addFd(fds[0], EPOLLIN, [&](uint32_t) {
        char buf[256];
        ssize_t n = read(fds[0], buf, sizeof(buf));
        if (n > 0) received.assign(buf, n);
    });

    std::thread t([&]() { loop.loop(); });

    write(fds[1], "eventloop_test", 14);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT(received == "eventloop_test", "read data in callback matches sent data");

    loop.stop();
    t.join();
    close(fds[0]);
    close(fds[1]);
}

void test_ET_mode_basic() {
    EventLoop loop(EventLoop::TriggerMode::ET);

    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    std::atomic<bool> fired{false};

    loop.addFd(fds[0], EPOLLIN, [&](uint32_t) { fired = true; });

    std::thread t([&]() { loop.loop(); });

    write(fds[1], "et", 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT(fired.load(), "ET mode: callback fired on first write");

    loop.stop();
    t.join();
    close(fds[0]);
    close(fds[1]);
}

void test_empty_loop_exits_on_stop() {
    EventLoop loop(EventLoop::TriggerMode::LT);

    std::thread t([&]() { loop.loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    loop.stop();
    t.join();

    EXPECT(true, "empty loop exits cleanly on stop()");
}

int main() {
    std::cout << "=== EventLoop Tests ===" << std::endl;

    test_construct_destruct();
    test_addFd_and_EPOLLIN();
    test_modFd();
    test_removeFd();
    test_stop_from_inside_callback();
    test_multiple_events();
    test_read_data_in_callback();
    test_ET_mode_basic();
    test_empty_loop_exits_on_stop();

    std::cout << "Passed: " << g_pass << ", Failed: " << g_fail << std::endl;
    return g_fail > 0 ? 1 : 0;
}
