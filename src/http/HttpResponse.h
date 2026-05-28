#pragma once

#include <string>
#include <unordered_map>

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
    void setContentLength(size_t len);

    void setBody(const std::string& body);
    void setBody(const char* data, size_t len);
    const std::string& body() const;

    void setKeepAlive(bool keepAlive);
    bool keepAlive() const;

    void setChunked(bool chunked);
    bool isChunked() const;

    void reset();

    std::string build() const;

private:
    int statusCode_;
    std::string statusMessage_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    bool keepAlive_;
    bool chunked_;
};

} // namespace ezNet
