#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "core/Connection.h"
#include "core/EventLoop.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "util/ThreadPool.h"

namespace ezNet {

/// 配置结构体（从 JSON 配置文件解析）
struct EzdropConfig {
    uint16_t port = 8080;
    std::string storageDir = "./data";
    size_t maxFileSizeMb = 0;         // 0 = 不限制
    int maxConcurrentDownloads = 0;   // 0 = 不限制
    int defaultExpireMinutes = 10;
    int maxExpireMinutes = 1440;
};

/// 取件码对应的文件元数据
struct FileMeta {
    std::string code;          // 6 位取件码
    std::string storagePath;   // 落盘文件路径（payload 或 payload.tar.gz）
    std::string originalName;  // 原始文件名/下载展示名
    uint64_t fileSize = 0;     // 字节
    int64_t expireAt = 0;      // unix ms
    std::atomic<int> downloadCount{0};
};

/// multipart 解析后的单个区块
struct MultipartPart {
    std::string name;      // form field name
    std::string filename;  // original filename (empty if form field)
    std::string contentType;
    std::string data;      // raw content
};

/// ezdrop 业务层：管理取件码表、上传/下载路由、过期清理
class EzdropServer {
public:
    EzdropServer(EventLoop* loop, std::string storageDir, uint16_t port,
                 std::string staticDir = "");
    ~EzdropServer();

    // ---- 配置文件支持 ----
    /// 从 JSON 配置文件加载设置，应用运行时参数（maxFileSize, maxConcurrentDownloads）
    void loadConfig(const std::string& configPath);
    /// 静态方法：读取 JSON 配置文件并返回配置结构体（创建服务器前调用）
    static EzdropConfig loadConfigFile(const std::string& configPath);

    // ---- 路由处理 ----
    /// POST /upload —— 流式落盘 + 生成取件码
    void handleUpload(const HttpRequest& req, HttpResponse* resp);

    /// GET /d/<code> —— 凭取件码下载，sendfile 零拷贝
    void handleDownload(const HttpRequest& req, HttpResponse* resp,
                        std::shared_ptr<Connection> conn);

    /// GET / —— 首页 HTML（上传 + 取件入口同一页）
    void handleIndex(HttpResponse* resp);

    /// GET /api/meta/<code> —— 查询取件码对应文件信息（前端校验用）
    void handleQuery(const HttpRequest& req, HttpResponse* resp);

    /// GET /api/stats —— 返回累计统计信息
    void handleStats(HttpResponse* resp);

    /// 启动后台过期清理任务
    void startExpirySweeper();

    // ---- 配置存取 ----
    uint16_t port() const { return port_; }
    const std::string& storageDir() const { return storageDir_; }

    void setMaxFileSize(size_t maxBytes) { maxFileSize_ = maxBytes; }
    void setMaxConcurrentDownloads(int max) { maxConcurrentDownloads_ = max; }

    /// 写入完成回调钩子（由 HttpServer 触发）
    void onWriteComplete(std::shared_ptr<Connection> conn);

private:
    /// 生成不冲突的 6 位数字取件码
    std::string generateCode();

    /// 扫描并删除过期文件
    void sweepExpired();

    // 路径安全检查：安全化文件名，防止路径遍历攻击
    // keepStructure=true 时保留相对路径结构（目录上传模式）
    std::string sanitizeFilename(const std::string& filename, bool keepStructure = false);

    // multipart 解析
    std::string extractBoundary(const std::string& contentType);
    std::vector<MultipartPart> parseMultipart(const std::string& body,
                                               const std::string& boundary);

    // 文件系统工具
    std::string randomSuffix();
    bool writeToFile(const std::string& path, const char* data, size_t len);
    bool removeDir(const std::string& path);

    // 打包
    bool createTarGz(const std::string& outputPath,
                     const std::string& sourceDir,
                     const std::string& originalName);

    // ---- JSON 解析（简易，仅支持该配置文件格式） ----
    /// 解析 JSON 配置文件，返回配置结构体
    static EzdropConfig parseConfigJson(const std::string& json);
    /// 跳过空白字符
    static void skipWhitespace(const std::string& s, size_t& pos);
    /// 解析 JSON 字符串值（跳过开头的 "）
    static std::string parseJsonString(const std::string& s, size_t& pos);
    /// 解析 JSON 数字值
    static int64_t parseJsonNumber(const std::string& s, size_t& pos);
    /// 解析 JSON 值（顶层入口）
    static void parseJsonValue(const std::string& s, size_t& pos,
                               const std::string& key, EzdropConfig& cfg);

    EventLoop* loop_;
    std::string storageDir_;
    std::string staticDir_;
    uint16_t port_;
    ThreadPool diskPool_{4, 100};
    std::atomic<bool> running_{true};

    // 取件码表，受 mutex 保护
    std::mutex metaMutex_;
    std::unordered_map<std::string, std::shared_ptr<FileMeta>> codes_;

    // 后台过期清理线程（join 在析构中）
    std::thread sweeperThread_;

    // ---- M3 功能成员 ----
    // 功能 2: 最大文件大小限制
    size_t maxFileSize_ = 0;  // 0 表示不限制

    // 功能 3: 并发下载限制
    int maxConcurrentDownloads_ = 0;    // 0 表示不限制
    std::atomic<int> activeDownloads_{0};
    std::mutex dlMapMutex_;
    std::unordered_map<int, uint64_t> activeDownloadSizes_;  // fd -> fileSize

    // 功能 4: 累计统计
    std::atomic<uint64_t> totalBytesUploaded_{0};
    std::atomic<uint64_t> totalBytesDownloaded_{0};
    std::atomic<uint64_t> totalFilesUploaded_{0};
    std::atomic<uint64_t> totalFilesDownloaded_{0};
};

} // namespace ezNet