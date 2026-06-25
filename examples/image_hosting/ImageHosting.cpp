#include "ImageHosting.h"
#include <fstream>
#include <sstream>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace ezNet {

ImageHosting::ImageHosting(EventLoop* loop, const std::string& storageDir)
    : loop_(loop), storageDir_(storageDir) {
    // 确保存储目录存在
    mkdir(storageDir_.c_str(), 0755);
}

std::string ImageHosting::generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    std::string uuid(32, '0');
    for (int i = 0; i < 32; ++i) uuid[i] = hex[dis(gen)];
    return uuid;
}

std::string ImageHosting::getExt(const std::string& contentType) {
    if (contentType == "image/jpeg") return ".jpg";
    if (contentType == "image/png")  return ".png";
    if (contentType == "image/gif")  return ".gif";
    if (contentType == "image/webp") return ".webp";
    if (contentType == "image/bmp")  return ".bmp";
    return ".bin";  // 默认二进制
}

void ImageHosting::handleUpload(const HttpRequest& req, HttpResponse* resp) {
    // consumeBody 将 body 移出，避免拷贝给线程池
    // req 虽然是 const&，但消费 body 是逻辑上的所有权转移
    std::string body = const_cast<HttpRequest&>(req).consumeBody();
    if (body.empty()) {
        resp->setStatusCode(400);
        resp->setBody("{\"error\":\"empty body\"}");
        return;
    }

    std::string ext = getExt(req.header("Content-Type"));
    std::string uuid = generateUUID();
    std::string prefix = uuid.substr(0, 2);
    std::string dir = storageDir_ + "/" + prefix;
    std::string filename = uuid + ext;
    std::string filepath = dir + "/" + filename;

    // 创建子目录并异步写文件
    mkdir(dir.c_str(), 0755);

    // 数据已经通过 consumeBody 取出，直接 move 给线程池
    uploadPool_.enqueue([filepath, body = std::move(body)]() {
        std::ofstream file(filepath, std::ios::binary);
        if (!file) return;
        file.write(body.data(), body.size());
        file.close();
    });

    // 同步返回 URL
    std::string url = "/img/" + filename;
    resp->setBody("{\"url\":\"" + url + "\",\"filename\":\"" + filename + "\"}");
}

} // namespace ezNet
