#include "core/Buffer.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

using namespace ezNet;

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; g_fail++; } \
    else { g_pass++; } \
} while(0)

void test_default_constructor() {
    Buffer buf;
    EXPECT(buf.readableBytes() == 0, "default: readable=0");
    EXPECT(buf.prependableBytes() == Buffer::kPrependSize, "default: prependable=kPrependSize");
    EXPECT(buf.writableBytes() > 0, "default: has writable space");
}

void test_custom_constructor() {
    Buffer buf(1024);
    EXPECT(buf.writableBytes() == 1024, "custom size: writable matches");
}

void test_append_and_retrieve() {
    Buffer buf;
    const char* data = "hello buffer";
    buf.append(data, strlen(data));

    EXPECT(buf.readableBytes() == strlen(data), "append: readable matches len");
    EXPECT(memcmp(buf.peek(), data, strlen(data)) == 0, "append: peek matches data");

    buf.retrieve(5);
    EXPECT(buf.readableBytes() == 7, "retrieve(5): readable decreased");

    char tmp[16] = {0};
    memcpy(tmp, buf.peek(), buf.readableBytes());
    EXPECT(strcmp(tmp, " buffer") == 0, "retrieve(5): remaining data correct");
}

void test_retrieve_all() {
    Buffer buf;
    buf.append("data");
    EXPECT(buf.readableBytes() == 4, "before retrieveAll");
    buf.retrieveAll();
    EXPECT(buf.readableBytes() == 0, "after retrieveAll: readable=0");
    EXPECT(buf.prependableBytes() == Buffer::kPrependSize, "after retrieveAll: prependable reset");
}

void test_retrieve_overflow() {
    Buffer buf;
    buf.append("abc");
    size_t before = buf.readableBytes();
    buf.retrieve(999);
    EXPECT(buf.readableBytes() == before, "retrieve overflow: no change when len > readable");
}

void test_retrieve_as_string() {
    Buffer buf;
    buf.append("hello world");
    std::string s = buf.retrieveAsString(5);
    EXPECT(s == "hello", "retrieveAsString: content matches");
    EXPECT(buf.readableBytes() == 6, "retrieveAsString: readable decreased");
}

void test_retrieve_all_as_string() {
    Buffer buf;
    buf.append("test");
    std::string s = buf.retrieveAllAsString();
    EXPECT(s == "test", "retrieveAllAsString: content matches");
    EXPECT(buf.readableBytes() == 0, "retrieveAllAsString: readable=0");
}

void test_append_string() {
    Buffer buf;
    std::string s = "cpp string";
    buf.append(s);
    EXPECT(buf.readableBytes() == s.size(), "append string: readable matches");
    EXPECT(memcmp(buf.peek(), s.data(), s.size()) == 0, "append string: data matches");
}

void test_prepend() {
    Buffer buf;
    buf.append("world");
    const char* head = "hello ";
    buf.prepend(head, strlen(head));
    EXPECT(buf.readableBytes() == 11, "prepend: total readable");
    EXPECT(memcmp(buf.peek(), "hello ", 6) == 0, "prepend: 'hello ' at front");
}

void test_prepend_overflow() {
    Buffer buf;
    char big[1024];
    memset(big, 'x', sizeof(big));
    buf.prepend(big, sizeof(big));
    EXPECT(buf.readableBytes() == 0, "prepend overflow: should be ignored");
}

void test_ensure_writable_no_move() {
    Buffer buf;
    size_t before = buf.writableBytes();
    buf.ensureWritableBytes(1);
    EXPECT(buf.writableBytes() == before, "ensureWritable: no resize when enough space");
}

void test_ensure_writable_expand() {
    Buffer buf(4);
    buf.append("abcd");
    buf.ensureWritableBytes(100);
    EXPECT(buf.writableBytes() >= 100, "ensureWritable: expanded enough");
    EXPECT(buf.readableBytes() == 4, "ensureWritable: readable preserved after expand");
}

void test_ensure_writable_compact() {
    Buffer buf(32);
    buf.append("hello");
    buf.retrieve(3);
    buf.append("world world world");
    EXPECT(memcmp(buf.peek(), "loworld world world", 19) == 0, "ensureWritable: compacted data intact");
}

void test_shrink() {
    Buffer buf(4096);
    buf.append("hello");
    buf.shrink(16);
    EXPECT(buf.writableBytes() >= 16, "shrink: writable >= reserve");
    EXPECT(buf.readableBytes() == 5, "shrink: readable preserved");
    EXPECT(memcmp(buf.peek(), "hello", 5) == 0, "shrink: data preserved");
}

void test_read_write_fd() {
    int fds[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    EXPECT(ret == 0, "socketpair created");

    const char* msg = "network buffer test!";
    ssize_t n = write(fds[1], msg, strlen(msg));
    EXPECT(n == (ssize_t)strlen(msg), "write to socketpair");

    Buffer buf;
    int savedErrno = 0;
    n = buf.readFromFd(fds[0], &savedErrno);
    EXPECT(n == (ssize_t)strlen(msg), "readFromFd: correct byte count");
    EXPECT(buf.readableBytes() == size_t(n), "readFromFd: readable matches");
    EXPECT(memcmp(buf.peek(), msg, n) == 0, "readFromFd: data matches");

    int savedErrno2 = 0;
    n = buf.writeToFd(fds[1], &savedErrno2);
    EXPECT(n == (ssize_t)strlen(msg), "writeToFd: correct byte count");
    EXPECT(buf.readableBytes() == 0, "writeToFd: data consumed");

    close(fds[0]);
    close(fds[1]);
}

void test_read_from_fd_large() {
    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    char big[40000];
    memset(big, 'A', sizeof(big));
    write(fds[1], big, sizeof(big));

    Buffer buf(16);
    int savedErrno = 0;
    ssize_t n = buf.readFromFd(fds[0], &savedErrno);
    EXPECT(n == (ssize_t)sizeof(big), "readFromFd: large data read correctly");
    EXPECT(buf.readableBytes() == (size_t)n, "readFromFd: large data readable matches");

    close(fds[0]);
    close(fds[1]);
}

void test_multiple_reads() {
    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    write(fds[1], "first", 5);
    write(fds[1], "second", 6);

    Buffer buf;
    int savedErrno = 0;
    buf.readFromFd(fds[0], &savedErrno);
    EXPECT(buf.readableBytes() == 11, "multiple reads: all data in buffer");

    close(fds[0]);
    close(fds[1]);
}

void test_begin_write() {
    Buffer buf;
    char* p = buf.beginWrite();
    memcpy(p, "data", 4);
    EXPECT(buf.readableBytes() == 0, "beginWrite direct: readable unchanged before append");
    buf.append("extra", 5);
    EXPECT(buf.readableBytes() == 5, "beginWrite: readable after proper append");
}

int main() {
    std::cout << "=== Buffer Tests ===" << std::endl;

    test_default_constructor();
    test_custom_constructor();
    test_append_and_retrieve();
    test_retrieve_all();
    test_retrieve_overflow();
    test_retrieve_as_string();
    test_retrieve_all_as_string();
    test_append_string();
    test_prepend();
    test_prepend_overflow();
    test_ensure_writable_no_move();
    test_ensure_writable_expand();
    test_ensure_writable_compact();
    test_shrink();
    test_read_write_fd();
    test_read_from_fd_large();
    test_multiple_reads();
    test_begin_write();

    std::cout << "Passed: " << g_pass << ", Failed: " << g_fail << std::endl;
    return g_fail > 0 ? 1 : 0;
}
