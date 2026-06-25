#include "core/Buffer.h"
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <sys/uio.h>
#include <unistd.h>

namespace ezNet {

Buffer::Buffer()
    : buffer_(kInitialSize + kPrependSize)
    , readerIndex_(kPrependSize)
    , writerIndex_(kPrependSize) {

}

Buffer::Buffer(size_t initialSize)
    : buffer_(initialSize + kPrependSize)
    , readerIndex_(kPrependSize)
    , writerIndex_(kPrependSize) {

}
// 可读字节数
size_t Buffer::readableBytes() const {
    return writerIndex_ - readerIndex_;
}
// 可写字节数
size_t Buffer::writableBytes() const {
    return buffer_.size() - writerIndex_;
}
// 可预写字节数
size_t Buffer::prependableBytes() const {
    return readerIndex_;
}
// 可读数据指针
const char* Buffer::peek() const {
    return buffer_.data() + readerIndex_;
}
// 可写数据指针
const char* Buffer::beginWrite() const {
    return buffer_.data() + writerIndex_;
}

char* Buffer::beginWrite() {
    return buffer_.data() + writerIndex_;
}
// 读取并移除len字节
void Buffer::retrieve(size_t len) {
    if(len <= readableBytes()) {
        readerIndex_ += len;
    }
}
// 读取并移除所有可读数据
void Buffer::retrieveAll() {
    readerIndex_ = writerIndex_ = kPrependSize;
}
// 读取并移除len字节，返回string
std::string Buffer::retrieveAsString(size_t len) {
    std::string str(peek(), len);
    retrieve(len);
    return str;
}
// 读取并移除所有可读数据，返回string
std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}
//往buffer写len字节的data
void Buffer::append(const char* data, size_t len) {
    ensureWritableBytes(len);
    memcpy(beginWrite(), data, len);
    writerIndex_ += len;
}
//往buffer写str
void Buffer::append(const std::string& str) {
    append(str.data(), str.size());
}

void Buffer::prepend(const void* data, size_t len) {
    if (prependableBytes() < len) return;
    memcpy(buffer_.data() + readerIndex_ - len, data, len);
    readerIndex_ -= len;
}

void Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() >= len) return;

    size_t readable = readableBytes();
    if (kPrependSize + readable + len <= buffer_.size()) {//碎片空间足够
        memmove(buffer_.data() + kPrependSize, peek(), readable);
        readerIndex_ = kPrependSize;
        writerIndex_ = readerIndex_ + readable;
    } else {//不够则扩容
        buffer_.resize(writerIndex_ + len);
    }
}

void Buffer::shrink(size_t reserve) {
    buffer_.resize(writerIndex_ + reserve);
    buffer_.shrink_to_fit(); 
}

ssize_t Buffer::readFromFd(int fd, int* savedErrno) {
    char extrabuf[65536];
    size_t writable = writableBytes();
    struct iovec iov[2] = {
        { beginWrite(), writable },
        { extrabuf, sizeof(extrabuf) }
    };
    ssize_t n = readv(fd, iov, 2);
    if (n < 0) {
        *savedErrno = errno;  // 保存 errno（EAGAIN 或真实错误）
        return -1;
    }
    if (n == 0) {
        return 0;  // EOF
    }
    if (static_cast<size_t>(n) <= writable) {//全部写入buffer_
        writerIndex_ += n;
    } else {//溢出部分写入栈缓冲区，重新写入buffer_
        writerIndex_ += writable;
        append(extrabuf, n - writable);
    }
    return n;
}

ssize_t Buffer::writeToFd(int fd, int* savedErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *savedErrno = errno;  // 保存 errno（EAGAIN 或真实错误）
        return -1;
    }
    if (n == 0) {
        return 0;  // 写了 0 字节（不太常见但可能）
    }
    retrieve(n);
    return n;
}

} // namespace ezNet
