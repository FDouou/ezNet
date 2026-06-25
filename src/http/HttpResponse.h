#pragma once

#include <string>
#include <vector>

namespace ezNet {

class HttpResponse {
public:
    HttpResponse();

    void setStatusCode(int code);
    int statusCode() const;

    void setStatusMessage(const std::string& msg);
    const std::string& statusMessage() const;

    void setContentType(const std::string& type);

    void addHeader(const std::string& name, const std::string& value);
    std::string header(const std::string& name) const;
    void setContentLength(size_t len);

    /// 设置 206 Partial Content 的 Content-Range 头
    void setContentRange(int64_t start, int64_t end, int64_t total);

    void setBody(const std::string& body);
    void setBody(const char* data, size_t len);
    const std::string& body() const;

    /// 设置文件响应（配合 Connection::sendFile 使用），返回 true 表示成功，false 表示文件不存在或无法访问
    bool setFile(const std::string& filePath);
    bool isFile() const { return isFile_; }
    const std::string& filePath() const { return filePath_; }
    size_t fileSize() const { return fileSize_; }

    void setKeepAlive(bool keepAlive);
    bool keepAlive() const;

    void setChunked(bool chunked);
    bool isChunked() const;

    void reset();

    std::string build() const;

private:
    int statusCode_;
    std::string statusMessage_;
    std::vector<std::pair<std::string, std::string>> headers_;
    std::string body_;
    bool keepAlive_;
    bool chunked_;

    // 文件响应相关
    bool isFile_ = false;
    std::string filePath_;
    size_t fileSize_ = 0;
};

} // namespace ezNet
