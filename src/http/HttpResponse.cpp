#include "http/HttpResponse.h"
#include <cctype>
#include <sstream>
#include <cstdio>
#include <sys/stat.h>

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

std::string HttpResponse::header(const std::string& name) const {
    for (auto& kv : headers_) {
        if (kv.first.size() == name.size()) {
            bool match = true;
            for (size_t i = 0; i < name.size(); i++) {
                if (std::tolower(static_cast<unsigned char>(kv.first[i])) !=
                    std::tolower(static_cast<unsigned char>(name[i]))) {
                    match = false;
                    break;
                }
            }
            if (match) return kv.second;
        }
    }
    return "";
}

void HttpResponse::setContentLength(size_t len) {
    addHeader("Content-Length", std::to_string(len));
}

void HttpResponse::setContentRange(int64_t start, int64_t end, int64_t total) {
    statusCode_ = 206;
    statusMessage_ = "Partial Content";
    addHeader("Content-Range", "bytes " + std::to_string(start) + "-" +
                                std::to_string(end) + "/" + std::to_string(total));
    setContentLength(static_cast<size_t>(end - start + 1));
}

void HttpResponse::setBody(const std::string& body) {
    body_ = body;
    setContentLength(body_.size());
}

void HttpResponse::setBody(const char* data, size_t len) {
    body_ = std::string(data, len);
    setContentLength(len);
}

bool HttpResponse::setFile(const std::string& filePath) {
    // 获取文件信息
    struct stat st;
    if (stat(filePath.c_str(), &st) != 0) {
        // stat 失败，不设置为文件响应
        isFile_ = false;
        statusCode_ = 404;
        statusMessage_ = "Not Found";
        return false;
    }

    isFile_ = true;
    filePath_ = filePath;
    fileSize_ = st.st_size;
    setContentLength(fileSize_);

    // 根据后缀设置 Content-Type（大小写不敏感）
    size_t dot = filePath.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = filePath.substr(dot);
        // 将扩展名转为小写以支持 .JPG、.JPEG 等
        for (auto& ch : ext) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (ext == ".jpg" || ext == ".jpeg")
            setContentType("image/jpeg");
        else if (ext == ".png")
            setContentType("image/png");
        else if (ext == ".gif")
            setContentType("image/gif");
        else if (ext == ".webp")
            setContentType("image/webp");
        else if (ext == ".bmp")
            setContentType("image/bmp");
        else if (ext == ".html" || ext == ".htm")
            setContentType("text/html");
        else if (ext == ".txt")
            setContentType("text/plain");
        else if (ext == ".css")
            setContentType("text/css");
        else if (ext == ".js")
            setContentType("application/javascript");
        else if (ext == ".json")
            setContentType("application/json");
        else if (ext == ".pdf")
            setContentType("application/pdf");
        else if (ext == ".zip")
            setContentType("application/zip");
        else
            setContentType("application/octet-stream");
    }
    return true;
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
    isFile_ = false;
    filePath_.clear();
    fileSize_ = 0;
}

std::string HttpResponse::build() const {
    std::string response;
    response.reserve(1024 + body_.size());
    response += "HTTP/1.1 " + std::to_string(statusCode_) + " " + statusMessage_ + "\r\n";
    response += "Server: ezNet\r\n";

    for (const auto& header : headers_) {
        if (chunked_ && header.first == "Content-Length") continue;
        response += header.first + ": " + header.second + "\r\n";
    }

    if (chunked_ && !body_.empty()) {
        response += "Transfer-Encoding: chunked\r\n";
        response += "\r\n";
        char hexBuf[32];
        snprintf(hexBuf, sizeof(hexBuf), "%zx\r\n", body_.size());
        response += hexBuf;
        response += body_;
        response += "\r\n0\r\n\r\n";
    } else {
        response += "\r\n";
        response += body_;
    }
    return response;
}

} // namespace ezNet
