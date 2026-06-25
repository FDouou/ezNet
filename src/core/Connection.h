#pragma once

#include <functional>
#include <memory>
#include <string>
#include <sys/types.h>
#include "core/Buffer.h"
#include "core/EventLoop.h"

namespace ezNet {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    enum class State {
        ReadingRequest,
        Processing,
        SendingResponse,
        Closing
    };

    using DataCallback = std::function<void(const std::shared_ptr<Connection>&, Buffer*)>;
    using CloseCallback = std::function<void(const std::shared_ptr<Connection>&)>;
    using WriteCompleteCallback = std::function<void(const std::shared_ptr<Connection>&)>;

    Connection(EventLoop* loop, int fd);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    void start();
    void close();
    void forceClose();

    void send(const std::string& data);
    void send(const char* data, size_t len);

    // 文件发送（sendfile 零拷贝），支持偏移量和可选长度
    void sendFile(const std::string& filePath, size_t fileSize, off_t offset = 0, size_t length = 0);

    // 文件发送（sendfile 零拷贝），使用已打开的文件 fd（调用方负责在失败时关闭，成功后由 Connection 接管）
    void sendFile(int fileFd, size_t fileSize, off_t offset = 0, size_t length = 0);

    void setDataCallback(DataCallback cb);
    void setCloseCallback(CloseCallback cb);
    const CloseCallback& closeCallback() const;
    void setWriteCompleteCallback(WriteCompleteCallback cb);

    int fd() const;
    State state() const;
    EventLoop* loop() const;
    Buffer& inputBuffer();
    Buffer& outputBuffer();

    bool isKeepAlive() const;
    void setKeepAlive(bool keepAlive);

    void setState(State s);
    void resetForNextRequest();

    void setUserData(void* data) { userData_ = data; }
    void* userData() const { return userData_; }

    void setWheelEntry(void* entry) { wheelEntry_ = entry; }
    void* wheelEntry() const { return wheelEntry_; }

private:
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    void shutdownWrite();

    EventLoop* loop_;
    int fd_;
    State state_;
    bool keepAlive_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;

    DataCallback dataCallback_;
    CloseCallback closeCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    void* userData_ = nullptr;
    void* wheelEntry_ = nullptr;

    // 文件发送（sendfile）
    int fileFd_ = -1;
    size_t fileSentSize_ = 0;   // 已发送的字节数（相对于 fileOffset_）
    size_t fileSize_ = 0;       // 文件总大小
    off_t fileOffset_ = 0;      // sendfile 起始偏移量
    size_t bytesToSend_ = 0;    // 需要发送的总字节数（0 表示到文件末尾）
    bool sendingFile_ = false;
};

} // namespace ezNet
