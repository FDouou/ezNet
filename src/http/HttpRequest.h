#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

namespace ezNet {

class Router;

class HttpRequest {
    friend class Router;
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

    const std::vector<std::pair<std::string, std::string>>& headers() const;
    std::string header(const std::string& name) const;
    void setHeader(const std::string& name, const std::string& value);

    const std::string& body() const;
    /// 消费 body：将 body 移出，避免拷贝（用于异步传递大数据）
    std::string consumeBody();
    void setBody(const std::string& body);
    void appendBody(const char* data, size_t len);

    bool keepAlive() const;
    void setKeepAlive(bool keepAlive);

    std::string pathParam(const std::string& name) const;
    int pathParamAsInt(const std::string& name, int defaultVal = 0) const;
    double pathParamAsDouble(const std::string& name, double defaultVal = 0.0) const;
    void setPathParam(const std::string& name, const std::string& value);

    int httpMajor() const;
    int httpMinor() const;
    void setHttpVersion(int major, int minor);

    // Range 请求（断点续传）
    bool hasRange() const;
    int64_t rangeStart() const;
    int64_t rangeEnd() const;
    void parseRange();

private:
    std::string method_;
    std::string url_;
    std::string path_;
    std::string queryString_;
    std::vector<std::pair<std::string, std::string>> headers_;
    std::string body_;
    bool keepAlive_;
    int httpMajor_;
    int httpMinor_;
    std::unordered_map<std::string, std::string> pathParams_;

    // Range 请求相关
    int64_t rangeStart_ = 0;
    int64_t rangeEnd_ = -1;    // -1 表示到文件末尾
    bool hasRange_ = false;
};

} // namespace ezNet
