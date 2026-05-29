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

} // namespace ezNet
