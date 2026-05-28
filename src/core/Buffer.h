#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <sys/types.h>

namespace ezNet {

class Buffer {
public:
    static constexpr size_t kInitialSize = 4096;
    static constexpr size_t kPrependSize = 8; //预留头部空间

    Buffer();
    explicit Buffer(size_t initialSize);

    size_t readableBytes() const;
    size_t writableBytes() const;
    size_t prependableBytes() const;

    const char* peek() const;
    const char* beginWrite() const;
    char* beginWrite();

    void retrieve(size_t len);
    void retrieveAll();
    std::string retrieveAsString(size_t len);
    std::string retrieveAllAsString();

    void append(const char* data, size_t len);
    void append(const std::string& str);
    void prepend(const void* data, size_t len);
    void ensureWritableBytes(size_t len);
    void shrink(size_t reserve);

    ssize_t readFromFd(int fd, int* savedErrno);
    ssize_t writeToFd(int fd, int* savedErrno);

private:
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

} // namespace ezNet
