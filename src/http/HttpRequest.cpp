#include "http/HttpRequest.h"
#include <algorithm>
#include <cctype>

namespace ezNet {

HttpRequest::HttpRequest()
    : keepAlive_(false), httpMajor_(1), httpMinor_(1) {
}

void HttpRequest::reset() {
    method_.clear();
    url_.clear();
    path_.clear();
    queryString_.clear();
    headers_.clear();
    body_.clear();
    keepAlive_ = false;
    httpMajor_ = 1;
    httpMinor_ = 1;
    pathParams_.clear();
    rangeStart_ = 0;
    rangeEnd_ = -1;
    hasRange_ = false;
}

std::string HttpRequest::method() const {
    return method_;
}

const std::string& HttpRequest::methodRef() const {
    return method_;
}

void HttpRequest::setMethod(const std::string& method) {
    method_ = method;
}

std::string HttpRequest::url() const {
    return url_;
}

const std::string& HttpRequest::urlRef() const {
    return url_;
}

void HttpRequest::setUrl(const std::string& url) {
    url_ = url;
    auto pos = url.find('?');
    if (pos != std::string::npos) {
        path_ = url.substr(0, pos);
        queryString_ = url.substr(pos + 1);
    } else {
        path_ = url;
        queryString_.clear();
    }
}

void HttpRequest::setUrl(const char* data, size_t len) {
    url_.assign(data, len);
    auto pos = url_.find('?');
    if (pos != std::string::npos) {
        path_ = url_.substr(0, pos);
        queryString_ = url_.substr(pos + 1);
    } else {
        path_ = url_;
        queryString_.clear();
    }
}

std::string HttpRequest::path() const {
    return path_;
}

const std::string& HttpRequest::pathRef() const {
    return path_;
}

void HttpRequest::setPath(const std::string& path) {
    path_ = path;
}

std::string HttpRequest::queryString() const {
    return queryString_;
}

const std::string& HttpRequest::queryStringRef() const {
    return queryString_;
}

const std::unordered_map<std::string, std::string>& HttpRequest::headers() const {
    return headers_;
}

std::string HttpRequest::header(const std::string& name) const {
    for (auto& kv : headers_) {
        if (kv.first.size() == name.size()) {
            bool match = true;
            for (size_t i = 0; i < name.size(); i++) {
                if (std::tolower(kv.first[i]) != std::tolower(name[i])) {
                    match = false;
                    break;
                }
            }
            if (match) return kv.second;
        }
    }
    return "";
}

void HttpRequest::setHeader(const std::string& name, const std::string& value) {
    headers_[name] = value;
}

const std::string& HttpRequest::body() const {
    return body_;
}

std::string HttpRequest::consumeBody() {
    return std::move(body_);
}

void HttpRequest::setBody(const std::string& body) {
    body_ = body;
}

void HttpRequest::appendBody(const char* data, size_t len) {
    body_.append(data, len);
}

bool HttpRequest::keepAlive() const {
    return keepAlive_;
}

void HttpRequest::setKeepAlive(bool keepAlive) {
    keepAlive_ = keepAlive;
}

int HttpRequest::httpMajor() const {
    return httpMajor_;
}

int HttpRequest::httpMinor() const {
    return httpMinor_;
}

void HttpRequest::setHttpVersion(int major, int minor) {
    httpMajor_ = major;
    httpMinor_ = minor;
}

std::string HttpRequest::pathParam(const std::string& name) const {
    auto it = pathParams_.find(name);
    if (it != pathParams_.end()) {
        return it->second;
    }
    return "";
}

void HttpRequest::setPathParam(const std::string& name, const std::string& value) {
    pathParams_[name] = value;
}

int HttpRequest::pathParamAsInt(const std::string& name, int defaultVal) const {
    auto it = pathParams_.find(name);
    if (it == pathParams_.end() || it->second.empty()) {
        return defaultVal;
    }
    try {
        return std::stoi(it->second);
    } catch (...) {
        return defaultVal;
    }
}

double HttpRequest::pathParamAsDouble(const std::string& name, double defaultVal) const {
    auto it = pathParams_.find(name);
    if (it == pathParams_.end() || it->second.empty()) {
        return defaultVal;
    }
    try {
        return std::stod(it->second);
    } catch (...) {
        return defaultVal;
    }
}

bool HttpRequest::hasRange() const {
    return hasRange_;
}

int64_t HttpRequest::rangeStart() const {
    return rangeStart_;
}

int64_t HttpRequest::rangeEnd() const {
    return rangeEnd_;
}

void HttpRequest::parseRange() {
    hasRange_ = false;
    rangeStart_ = 0;
    rangeEnd_ = -1;

    std::string rangeVal = header("Range");
    if (rangeVal.empty()) {
        return;
    }

    // 格式: bytes=START-END, bytes=START-, bytes=-SUFFIX
    const std::string prefix = "bytes=";
    if (rangeVal.size() <= prefix.size() ||
        rangeVal.compare(0, prefix.size(), prefix) != 0) {
        return;
    }

    std::string rangeSpec = rangeVal.substr(prefix.size());

    // 多段 Range（包含逗号）暂不支持
    if (rangeSpec.find(',') != std::string::npos) {
        return;
    }

    auto dashPos = rangeSpec.find('-');
    if (dashPos == std::string::npos) {
        return;
    }

    std::string startStr = rangeSpec.substr(0, dashPos);
    std::string endStr = rangeSpec.substr(dashPos + 1);

    // bytes=- 无效
    if (startStr.empty() && endStr.empty()) {
        return;
    }

    try {
        if (startStr.empty()) {
            // bytes=-SUFFIX：最后 SUFFIX 字节
            rangeStart_ = -1;  // 标记为 suffix 请求
            rangeEnd_ = std::stoll(endStr);
        } else {
            rangeStart_ = std::stoll(startStr);
            if (rangeStart_ < 0) {
                rangeStart_ = 0;
                rangeEnd_ = -1;
                return;
            }
            if (endStr.empty()) {
                // bytes=START-：从 START 到末尾
                rangeEnd_ = -1;
            } else {
                // bytes=START-END
                rangeEnd_ = std::stoll(endStr);
                if (rangeEnd_ < 0) {
                    rangeEnd_ = -1;
                }
            }
        }
        hasRange_ = true;
    } catch (...) {
        // 解析失败，忽略 Range
        hasRange_ = false;
        rangeStart_ = 0;
        rangeEnd_ = -1;
    }
}

} // namespace ezNet
