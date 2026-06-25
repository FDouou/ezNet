#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "core/EventLoop.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "util/ThreadPool.h"

namespace ezNet {

/// 简易图床业务逻辑
class ImageHosting {
public:
    ImageHosting(EventLoop* loop, const std::string& storageDir);

    /// POST /upload — 接收图片，保存到磁盘，返回 URL
    void handleUpload(const HttpRequest& req, HttpResponse* resp);

    const std::string& storageDir() const { return storageDir_; }

private:
    std::string generateUUID();
    std::string getContentType(const std::string& ext);
    std::string getExt(const std::string& contentType);

    EventLoop* loop_;
    std::string storageDir_;
    ThreadPool uploadPool_{2, 200};   // 上传写盘线程池
};

} // namespace ezNet
