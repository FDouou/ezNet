#include "http/HttpResponse.h"
#include <sstream>
#include <cstdio>

namespace ezNet {

HttpResponse::HttpResponse()
    : statusCode_(200), statusMessage_("OK"), keepAlive_(false), chunked_(false) {}

void HttpResponse::setStatusCode(int code) {
    statusCode_ = code;
}

int HttpResponse::statusCode() const {
    return statusCode_;
}

void HttpResponse::setStatusMessage(const std::string& msg) {
    statusMessage_ = msg;
}

const std::string& HttpResponse::statusMessage() const {
    return statusMessage_;
}

void HttpResponse::setContentType(const std::string& type) {
    addHeader("Content-Type", type);
}

void HttpResponse::addHeader(const std::string& name, const std::string& value) {
    headers_[name] = value;
}

void HttpResponse::setContentLength(size_t len) {
    addHeader("Content-Length", std::to_string(len));
}

void HttpResponse::setBody(const std::string& body) {
    body_ = body;
    setContentLength(body_.size());
}

void HttpResponse::setBody(const char* data, size_t len) {
    body_ = std::string(data, len);
    setContentLength(len);
}

const std::string& HttpResponse::body() const {
    return body_;
}

void HttpResponse::setKeepAlive(bool keepAlive) {
    keepAlive_ = keepAlive;
}

bool HttpResponse::keepAlive() const {
    return keepAlive_;
}

void HttpResponse::setChunked(bool chunked) {
    chunked_ = chunked;
}

bool HttpResponse::isChunked() const {
    return chunked_;
}

void HttpResponse::reset() {
    statusCode_ = 200;
    statusMessage_ = "OK";
    keepAlive_ = false;
    chunked_ = false;
    headers_.clear();
    body_.clear();
}

std::string HttpResponse::build() const {
    std::string response;
    response.reserve(1024);
    response += "HTTP/1.1 " + std::to_string(statusCode_) + " " + statusMessage_ + "\r\n";
    response += "Server: ezNet\r\n";
    for (const auto& header : headers_) {
        response += header.first + ": " + header.second + "\r\n";
    }
    response += "\r\n";
    response += body_;
    return response;
}

} // namespace ezNet
