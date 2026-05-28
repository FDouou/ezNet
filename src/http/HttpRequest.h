#pragma once

#include <string>
#include <unordered_map>

namespace ezNet {

class HttpRequest {
public:
    HttpRequest();

    void reset();

    std::string method() const;
    const std::string& methodRef() const;
    void setMethod(const std::string& method);

    std::string url() const;
    const std::string& urlRef() const;
    void setUrl(const std::string& url);
    void setUrl(const char* data, size_t len);

    std::string path() const;
    const std::string& pathRef() const;
    void setPath(const std::string& path);

    std::string queryString() const;
    const std::string& queryStringRef() const;

    const std::unordered_map<std::string, std::string>& headers() const;
    std::string header(const std::string& name) const;
    void setHeader(const std::string& name, const std::string& value);

    const std::string& body() const;
    void setBody(const std::string& body);
    void appendBody(const char* data, size_t len);

    bool keepAlive() const;
    void setKeepAlive(bool keepAlive);

    int httpMajor() const;
    int httpMinor() const;
    void setHttpVersion(int major, int minor);

private:
    std::string method_;
    std::string url_;
    std::string path_;
    std::string queryString_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    bool keepAlive_;
    int httpMajor_;
    int httpMinor_;
};

} // namespace ezNet
