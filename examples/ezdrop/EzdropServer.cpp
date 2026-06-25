#include "EzdropServer.h"
#include "util/Logger.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <random>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

namespace ezNet {

// ============ JSON 解析器（简易，仅支持配置文件格式） ============

void EzdropServer::skipWhitespace(const std::string& s, size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                              s[pos] == '\n' || s[pos] == '\r')) {
        ++pos;
    }
}

std::string EzdropServer::parseJsonString(const std::string& s, size_t& pos) {
    // pos 应该在引号处
    if (pos >= s.size() || s[pos] != '"') return "";
    ++pos; // 跳过开头引号
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\') {
            ++pos;
            if (pos >= s.size()) break;
            switch (s[pos]) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case '/':  result += '/'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u': {
                    // 简单的 unicode 转义，仅支持 u00xx 范围
                    if (pos + 4 < s.size()) {
                        // 粗略跳过 4 位 hex
                        pos += 4;
                    }
                    break;
                }
                default: result += s[pos]; break;
            }
        } else {
            result += s[pos];
        }
        ++pos;
    }
    if (pos < s.size() && s[pos] == '"') ++pos; // 跳过结尾引号
    return result;
}

int64_t EzdropServer::parseJsonNumber(const std::string& s, size_t& pos) {
    skipWhitespace(s, pos);
    int64_t val = 0;
    bool negative = false;
    if (pos < s.size() && s[pos] == '-') {
        negative = true;
        ++pos;
    }
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        val = val * 10 + (s[pos] - '0');
        ++pos;
    }
    if (negative) val = -val;
    return val;
}

void EzdropServer::parseJsonValue(const std::string& s, size_t& pos,
                                   const std::string& key, EzdropConfig& cfg) {
    skipWhitespace(s, pos);
    if (pos >= s.size()) return;

    if (key == "port") {
        cfg.port = static_cast<uint16_t>(parseJsonNumber(s, pos));
    } else if (key == "storage_dir") {
        cfg.storageDir = parseJsonString(s, pos);
    } else if (key == "max_file_size_mb") {
        cfg.maxFileSizeMb = static_cast<size_t>(parseJsonNumber(s, pos));
    } else if (key == "max_concurrent_downloads") {
        cfg.maxConcurrentDownloads = static_cast<int>(parseJsonNumber(s, pos));
    } else if (key == "default_expire_minutes") {
        cfg.defaultExpireMinutes = static_cast<int>(parseJsonNumber(s, pos));
    } else if (key == "max_expire_minutes") {
        cfg.maxExpireMinutes = static_cast<int>(parseJsonNumber(s, pos));
    } else {
        // 未知字段，跳过值
        if (pos < s.size() && s[pos] == '"') {
            parseJsonString(s, pos);
        } else {
            parseJsonNumber(s, pos);
        }
    }
}

EzdropConfig EzdropServer::parseConfigJson(const std::string& json) {
    EzdropConfig cfg;
    size_t pos = 0;
    skipWhitespace(json, pos);
    // 期望顶层对象 '{'
    if (pos >= json.size() || json[pos] != '{') return cfg;
    ++pos; // 跳过 '{'

    while (pos < json.size()) {
        skipWhitespace(json, pos);
        if (pos >= json.size()) break;
        if (json[pos] == '}') {
            ++pos;
            break;
        }
        if (json[pos] == ',') {
            ++pos;
            continue;
        }
        // 解析 key
        std::string key = parseJsonString(json, pos);
        skipWhitespace(json, pos);
        // 跳过 ':'
        if (pos < json.size() && json[pos] == ':') ++pos;
        skipWhitespace(json, pos);
        // 解析值
        parseJsonValue(json, pos, key, cfg);
        skipWhitespace(json, pos);
    }
    return cfg;
}

EzdropConfig EzdropServer::loadConfigFile(const std::string& configPath) {
    FILE* fp = fopen(configPath.c_str(), "rb");
    if (!fp) {
        LOG_WARN("Config file %s not found, using defaults", configPath.c_str());
        return EzdropConfig{};
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::string json;
    if (len > 0) {
        json.resize(static_cast<size_t>(len));
        size_t nread = fread(&json[0], 1, json.size(), fp);
        if (nread != static_cast<size_t>(len)) {
            json.resize(nread);
        }
    }
    fclose(fp);
    return parseConfigJson(json);
}

void EzdropServer::loadConfig(const std::string& configPath) {
    EzdropConfig cfg = loadConfigFile(configPath);

    // 仅设置运行时参数（port 和 storageDir 由调用方在创建服务器前处理）
    if (cfg.maxFileSizeMb > 0) {
        maxFileSize_ = cfg.maxFileSizeMb * 1024 * 1024;
    }
    if (cfg.maxConcurrentDownloads > 0) {
        maxConcurrentDownloads_ = cfg.maxConcurrentDownloads;
    }

    LOG_INFO("Config loaded: maxFileSize=%zu maxConcurrent=%d",
             maxFileSize_, maxConcurrentDownloads_);
}

// ============ 工具函数 ============

std::string EzdropServer::extractBoundary(const std::string& contentType) {
    const std::string prefix = "boundary=";
    auto pos = contentType.find(prefix);
    if (pos == std::string::npos) return "";
    std::string boundary = contentType.substr(pos + prefix.size());
    // 去掉可能的引号
    if (!boundary.empty() && boundary.front() == '"') {
        boundary.erase(0, 1);
        auto endQ = boundary.find('"');
        if (endQ != std::string::npos) boundary.resize(endQ);
    }
    return boundary;
}

std::vector<MultipartPart> EzdropServer::parseMultipart(const std::string& body,
                                                         const std::string& boundary) {
    std::vector<MultipartPart> parts;
    std::string delimiter = "--" + boundary;
    std::string endDelimiter = delimiter + "--";

    size_t pos = 0;
    while (pos < body.size()) {
        // 查找下一个 delimiter
        size_t delimStart = body.find(delimiter, pos);
        if (delimStart == std::string::npos) break;

        // 检查是否为结束 delimiter
        if (body.compare(delimStart, endDelimiter.size(), endDelimiter) == 0) break;

        // 跳过 delimiter 和 CRLF
        size_t partStart = delimStart + delimiter.size();
        if (partStart < body.size() && body[partStart] == '\r') partStart++;
        if (partStart < body.size() && body[partStart] == '\n') partStart++;

        // 查找头部结束（空行：\r\n\r\n）
        size_t headerEnd = body.find("\r\n\r\n", partStart);
        if (headerEnd == std::string::npos) break;

        std::string headersBlock = body.substr(partStart, headerEnd - partStart);

        // 数据起始位置
        size_t dataStart = headerEnd + 4;

        // 查找数据结束（下一个 delimiter 的 CRLF 之前）
        size_t dataEnd = body.find(delimiter, dataStart);
        if (dataEnd == std::string::npos) {
            dataEnd = body.size();
        }
        // 去掉尾部 CRLF（分隔符前面的）
        if (dataEnd >= 2 && body[dataEnd - 2] == '\r' && body[dataEnd - 1] == '\n') {
            dataEnd -= 2;
        }

        MultipartPart part;
        part.data = body.substr(dataStart, dataEnd - dataStart);

        // 解析头部：提取 name 和 filename
        auto extractHeaderValue = [](const std::string& headers,
                                     const std::string& key) -> std::string {
            auto keyPos = headers.find(key + "=\"");
            if (keyPos == std::string::npos) return "";
            size_t valStart = keyPos + key.size() + 2; // key="
            size_t valEnd = headers.find('"', valStart);
            if (valEnd == std::string::npos) return "";
            return headers.substr(valStart, valEnd - valStart);
        };

        part.name = extractHeaderValue(headersBlock, "name");
        part.filename = extractHeaderValue(headersBlock, "filename");

        // 提取 Content-Type（如果存在）
        auto ctPos = headersBlock.find("Content-Type:");
        if (ctPos != std::string::npos) {
            size_t ctValStart = ctPos + 13; // "Content-Type:"
            while (ctValStart < headersBlock.size() &&
                   (headersBlock[ctValStart] == ' ' || headersBlock[ctValStart] == '\t')) {
                ctValStart++;
            }
            size_t ctValEnd = headersBlock.find('\r', ctValStart);
            if (ctValEnd == std::string::npos) ctValEnd = headersBlock.find('\n', ctValStart);
            if (ctValEnd == std::string::npos) ctValEnd = headersBlock.size();
            part.contentType = headersBlock.substr(ctValStart, ctValEnd - ctValStart);
        }

        parts.push_back(std::move(part));
        pos = dataEnd;
    }

    return parts;
}

std::string EzdropServer::sanitizeFilename(const std::string& filename, bool keepStructure) {
    std::string name = filename;

    // 1. 将所有反斜杠转换为正斜杠（统一路径分隔符）
    for (auto& ch : name) {
        if (ch == '\\') ch = '/';
    }

    // 2. 检查路径遍历：拒绝包含 ".." 的路径
    if (name.find("..") != std::string::npos) {
        return "";
    }

    // 3. 去掉开头的 /（绝对路径）
    while (!name.empty() && name[0] == '/') {
        name.erase(0, 1);
    }

    if (keepStructure) {
        // 目录模式：保留相对路径结构，但检查每个路径段
        std::string result;
        size_t start = 0;
        while (start < name.size()) {
            size_t slash = name.find('/', start);
            std::string segment;
            if (slash == std::string::npos) {
                segment = name.substr(start);
                start = name.size();
            } else {
                segment = name.substr(start, slash - start);
                start = slash + 1;
            }
            // 跳过空段或 "." 段
            if (segment.empty() || segment == ".") continue;
            if (!result.empty()) result += '/';
            result += segment;
        }
        return result;
    } else {
        // 文件模式：仅保留 basename
        auto lastSlash = name.rfind('/');
        if (lastSlash != std::string::npos) {
            name = name.substr(lastSlash + 1);
        }
        if (name.empty() || name == ".") {
            return "";
        }
        return name;
    }
}

std::string EzdropServer::randomSuffix() {
    thread_local std::mt19937 gen{std::random_device{}()};
    thread_local const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    thread_local std::uniform_int_distribution<int> dis(0, sizeof(alphanum) - 2);
    std::string s;
    s.reserve(8);
    for (int i = 0; i < 8; ++i) {
        s += alphanum[dis(gen)];
    }
    return s;
}

bool EzdropServer::writeToFile(const std::string& path, const char* data, size_t len) {
    // 确保父目录存在
    auto lastSlash = path.rfind('/');
    if (lastSlash != std::string::npos) {
        std::string parent = path.substr(0, lastSlash);
        // 递归创建目录
        std::string cur;
        for (size_t i = 0; i < parent.size(); ++i) {
            cur += parent[i];
            if (parent[i] == '/' || i == parent.size() - 1) {
                mkdir(cur.c_str(), 0755);
            }
        }
    }

    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) {
        LOG_ERROR("writeToFile: cannot open %s", path.c_str());
        return false;
    }
    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);
    if (written != len) {
        LOG_ERROR("writeToFile: write %s failed (%zu/%zu)", path.c_str(), written, len);
        return false;
    }
    return true;
}

bool EzdropServer::removeDir(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        // 可能是个文件，直接删除
        if (::unlink(path.c_str()) == 0) return true;
        return false;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        std::string fullPath = path + "/" + entry->d_name;
        if (entry->d_type == DT_DIR) {
            removeDir(fullPath);
        } else {
            ::unlink(fullPath.c_str());
        }
    }
    closedir(dir);
    ::rmdir(path.c_str());
    return true;
}

bool EzdropServer::createTarGz(const std::string& outputPath,
                                const std::string& sourceDir,
                                const std::string& originalName) {
    (void)originalName;
    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("createTarGz: fork failed");
        return false;
    }
    if (pid == 0) {
        // 子进程：调用 /bin/tar 打包
        execlp("tar", "tar", "czf", outputPath.c_str(),
               "-C", sourceDir.c_str(), ".", (char*)nullptr);
        // 如果 execlp 返回，说明出错
        _exit(1);
    }
    // 父进程：等待子进程完成
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
    }
    LOG_ERROR("createTarGz: tar failed for %s", outputPath.c_str());
    return false;
}

// ============ 构造函数 / 析构函数 ============

EzdropServer::EzdropServer(EventLoop* loop, std::string storageDir, uint16_t port,
                           std::string staticDir)
    : loop_(loop), storageDir_(std::move(storageDir)), port_(port),
      staticDir_(std::move(staticDir)) {
    if (staticDir_.empty()) {
        staticDir_ = "examples/ezdrop/static";
    }
    mkdir(storageDir_.c_str(), 0755);
    LOG_INFO("EzdropServer created, storage: %s, static: %s, port: %u",
             storageDir_.c_str(), staticDir_.c_str(), port);
}

EzdropServer::~EzdropServer() {
    running_ = false;
    if (sweeperThread_.joinable()) {
        sweeperThread_.join();
    }
    // 关闭时清理所有本地存储文件
    std::lock_guard<std::mutex> lk(metaMutex_);
    for (auto& [code, meta] : codes_) {
        ::unlink(meta->storagePath.c_str());
        auto lastSlash = meta->storagePath.rfind('/');
        if (lastSlash != std::string::npos) {
            std::string codeDir = meta->storagePath.substr(0, lastSlash);
            removeDir(codeDir);
        }
    }
    size_t count = codes_.size();
    codes_.clear();
    LOG_INFO("EzdropServer shutdown: cleaned %zu stored files", count);
}

// ============ 取件码生成 ============

std::string EzdropServer::generateCode() {
    static std::mt19937 gen{std::random_device{}()};
    static std::uniform_int_distribution<int> dis(0, 999999);
    std::lock_guard<std::mutex> lk(metaMutex_);
    std::string code;
    do {
        char buf[8];
        snprintf(buf, sizeof(buf), "%06d", dis(gen));
        code = buf;
    } while (codes_.count(code) > 0);
    return code;
}

// ============ 首页 HTML ============

void EzdropServer::handleIndex(HttpResponse* resp) {
    std::string indexPath = staticDir_ + "/index.html";
    if (!resp->setFile(indexPath)) {
        // setFile 在失败时会将状态码设为 404，但我们返回 500
        resp->setStatusCode(500);
        resp->setStatusMessage("Internal Server Error");
        resp->setBody("Internal Server Error: index.html not found");
        resp->setContentType("text/html; charset=utf-8");
    } else {
        // setFile 已根据扩展名设置 Content-Type 为 text/html，
        // 需额外设置 charset=utf-8
        resp->setContentType("text/html; charset=utf-8");
    }
}

// ============ 查询 ============

void EzdropServer::handleQuery(const HttpRequest& req, HttpResponse* resp) {
    std::string code = req.pathParam("code");
    if (code.empty() || code.size() != 6) {
        resp->setStatusCode(400);
        resp->setContentType("application/json");
        resp->setBody("{\"error\":\"invalid code\"}");
        return;
    }
    std::lock_guard<std::mutex> lk(metaMutex_);
    auto it = codes_.find(code);
    if (it == codes_.end() || it->second->expireAt < std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) {
        resp->setStatusCode(404);
        resp->setContentType("application/json");
        resp->setBody("{\"error\":\"not found or expired\"}");
        return;
    }
    auto& m = it->second;
    resp->setContentType("application/json");
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"code\":\"%s\",\"name\":\"%s\",\"size\":%llu}",
             m->code.c_str(), m->originalName.c_str(),
             static_cast<unsigned long long>(m->fileSize));
    resp->setBody(buf);
}

// ============ 上传 ============

void EzdropServer::handleUpload(const HttpRequest& req, HttpResponse* resp) {
    resp->setContentType("application/json");

    // 1. 提取 boundary
    std::string contentType = req.header("content-type");
    std::string boundary = extractBoundary(contentType);
    if (boundary.empty()) {
        resp->setStatusCode(400);
        resp->setBody("{\"error\":\"invalid Content-Type, expected multipart/form-data\"}");
        return;
    }

    // 2. 解析 multipart body
    const std::string& body = req.body();
    if (body.empty()) {
        resp->setStatusCode(400);
        resp->setBody("{\"error\":\"empty request body\"}");
        return;
    }

    // M3: 检查请求体大小是否超过最大文件限制
    if (maxFileSize_ > 0 && body.size() > maxFileSize_) {
        resp->setStatusCode(413);
        resp->addHeader("Connection", "close");
        char errMsg[128];
        snprintf(errMsg, sizeof(errMsg),
                 "{\"error\":\"payload too large (%zu bytes), max allowed: %zu bytes\"}",
                 body.size(), maxFileSize_);
        resp->setBody(errMsg);
        LOG_WARN("Upload rejected: %zu bytes exceeds limit of %zu bytes",
                 body.size(), maxFileSize_);
        return;
    }

    auto parts = parseMultipart(body, boundary);
    if (parts.empty()) {
        resp->setStatusCode(400);
        resp->setBody("{\"error\":\"no parts found in multipart body\"}");
        return;
    }

    // 3. 提取过期时间、模式（mode）和文件部分
    int expireMinutes = 10; // 默认 10 分钟
    std::string uploadMode = "files"; // 默认文件模式
    std::vector<const MultipartPart*> fileParts;
    std::string originalName;
    uint64_t totalSize = 0;

    for (auto& part : parts) {
        if (part.name == "expire") {
            try {
                expireMinutes = std::stoi(part.data);
                if (expireMinutes < 1) expireMinutes = 1;
                if (expireMinutes > 1440) expireMinutes = 1440; // 最大 24h
            } catch (...) {
                expireMinutes = 10;
            }
        } else if (part.name == "mode") {
            uploadMode = part.data;
        } else if (part.name == "files" && !part.filename.empty()) {
            fileParts.push_back(&part);
            totalSize += part.data.size();
        }
    }

    if (fileParts.empty()) {
        resp->setStatusCode(400);
        resp->setBody("{\"error\":\"no files uploaded\"}");
        return;
    }

    // 自动检测上传模式（如果前端未提供 mode，则从文件名中推断）
    if (uploadMode == "files") {
        for (const auto* part : fileParts) {
            if (part->filename.find('/') != std::string::npos ||
                part->filename.find('\\') != std::string::npos) {
                uploadMode = "directory";
                break;
            }
        }
    }

    // 确定展示名称
    if (uploadMode == "directory") {
        // 目录模式：从第一个文件的路径中提取顶层目录名
        std::string firstPath = fileParts[0]->filename;
        auto firstSlash = firstPath.find('/');
        if (firstSlash != std::string::npos) {
            originalName = firstPath.substr(0, firstSlash);
        } else {
            // 如果没有斜杠，就用文件名本身
            originalName = firstPath;
        }
        originalName += ".tar.gz";
    } else if (fileParts.size() == 1) {
        // 文件模式 + 单文件：使用原名（basename）
        originalName = fileParts[0]->filename;
        auto lastSlash = originalName.rfind('/');
        if (lastSlash != std::string::npos) {
            originalName = originalName.substr(lastSlash + 1);
        }
        // Windows 反斜杠
        auto lastBackslash = originalName.rfind('\\');
        if (lastBackslash != std::string::npos) {
            originalName = originalName.substr(lastBackslash + 1);
        }
    } else {
        // 文件模式 + 多文件
        originalName = "files.tar.gz";
    }

    // 4. 创建临时目录写入文件
    std::string tmpDir = storageDir_ + "/tmp_" + randomSuffix();
    if (mkdir(tmpDir.c_str(), 0755) != 0) {
        resp->setStatusCode(500);
        resp->setBody("{\"error\":\"failed to create temp directory\"}");
        return;
    }
    LOG_INFO("Upload: created temp dir %s", tmpDir.c_str());

    // 5. 写入文件到临时目录（先消毒文件名防路径遍历）
    for (auto* part : fileParts) {
        std::string safeFilename = sanitizeFilename(part->filename, uploadMode == "directory");
        if (safeFilename.empty()) {
            removeDir(tmpDir);
            resp->setStatusCode(400);
            resp->setBody("{\"error\":\"invalid filename (path traversal detected)\"}");
            LOG_WARN("Upload: rejected unsafe filename: %s", part->filename.c_str());
            return;
        }
        std::string filePath = tmpDir + "/" + safeFilename;
        // 对于保留目录结构的情况，需要创建子目录
        if (uploadMode == "directory") {
            auto lastSlash = safeFilename.rfind('/');
            if (lastSlash != std::string::npos) {
                std::string parentDir = tmpDir + "/" + safeFilename.substr(0, lastSlash);
                // 递归创建目录
                std::string cur;
                for (size_t i = 0; i < parentDir.size(); ++i) {
                    cur += parentDir[i];
                    if (parentDir[i] == '/' || i == parentDir.size() - 1) {
                        mkdir(cur.c_str(), 0755);
                    }
                }
            }
        }
        if (!writeToFile(filePath, part->data.data(), part->data.size())) {
            removeDir(tmpDir);
            resp->setStatusCode(500);
            resp->setBody("{\"error\":\"failed to write uploaded file\"}");
            return;
        }
        LOG_INFO("Upload: wrote %s (%zu bytes)", safeFilename.c_str(), part->data.size());
    }

    // 6. 生成取件码
    std::string code = generateCode();
    std::string codeDir = storageDir_ + "/" + code;
    if (mkdir(codeDir.c_str(), 0755) != 0) {
        removeDir(tmpDir);
        resp->setStatusCode(500);
        resp->setBody("{\"error\":\"failed to create code directory\"}");
        return;
    }

    // 7. 打包/存储 payload
    bool singleFileMode = (uploadMode == "files" && fileParts.size() == 1);
    std::string storagePath;
    uint64_t archiveSize = totalSize;

    if (singleFileMode) {
        // 单文件模式：跳过 tar.gz，直接将 tmpDir 中的唯一文件移动到 codeDir/payload
        storagePath = codeDir + "/payload";
        // tmpDir 中只有一个文件，用 rename 系统调用移动
        std::string tmpFilePath = tmpDir + "/" + sanitizeFilename(fileParts[0]->filename);
        if (rename(tmpFilePath.c_str(), storagePath.c_str()) != 0) {
            removeDir(tmpDir);
            removeDir(codeDir);
            resp->setStatusCode(500);
            resp->setBody("{\"error\":\"failed to move uploaded file\"}");
            return;
        }
    } else {
        // 多文件或目录模式：打包为 tar.gz
        std::string tarPath = codeDir + "/payload.tar.gz";
        if (!createTarGz(tarPath, tmpDir, originalName)) {
            removeDir(tmpDir);
            removeDir(codeDir);
            resp->setStatusCode(500);
            resp->setBody("{\"error\":\"failed to create archive\"}");
            return;
        }
        storagePath = tarPath;

        // 获取打包后文件大小
        struct stat st;
        if (stat(tarPath.c_str(), &st) == 0) {
            archiveSize = static_cast<uint64_t>(st.st_size);
        }
    }

    // 8. 写入 meta.json
    std::string metaPath = codeDir + "/meta.json";
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t expireAt = nowMs + static_cast<int64_t>(expireMinutes) * 60 * 1000;

    std::ofstream metaFile(metaPath);
    metaFile << "{\n";
    metaFile << "  \"code\": \"" << code << "\",\n";
    metaFile << "  \"originalName\": \"" << originalName << "\",\n";
    metaFile << "  \"fileSize\": " << archiveSize << ",\n";
    metaFile << "  \"expireAt\": " << expireAt << "\n";
    metaFile << "}\n";
    metaFile.close();

    // 9. 存入内存表
    auto meta = std::make_shared<FileMeta>();
    meta->code = code;
    meta->storagePath = storagePath;
    meta->originalName = originalName;
    meta->fileSize = archiveSize;
    meta->expireAt = expireAt;
    meta->downloadCount = 0;

    {
        std::lock_guard<std::mutex> lk(metaMutex_);
        codes_[code] = meta;
    }

    // 11. 清理临时目录
    removeDir(tmpDir);

    // 12. 更新统计
    totalFilesUploaded_++;
    totalBytesUploaded_ += archiveSize;

    // 13. 返回取件码
    char buf[384];
    snprintf(buf, sizeof(buf),
             "{\"code\":\"%s\",\"name\":\"%s\",\"size\":%llu,\"expiresIn\":%d}",
             code.c_str(), originalName.c_str(),
             static_cast<unsigned long long>(archiveSize), expireMinutes);
    resp->setBody(buf);

    LOG_INFO("Upload: code=%s name=%s size=%llu expire=%dm",
             code.c_str(), originalName.c_str(),
             static_cast<unsigned long long>(archiveSize), expireMinutes);
}

// ============ 下载 ============

void EzdropServer::handleDownload(const HttpRequest& req, HttpResponse* resp,
                                   std::shared_ptr<Connection> conn) {
    std::string code = req.pathParam("code");
    if (code.empty() || code.size() != 6) {
        resp->setStatusCode(400);
        resp->setContentType("application/json");
        resp->setBody("{\"error\":\"invalid code\"}");
        return;
    }

    // M3: 检查并发下载限制
    if (maxConcurrentDownloads_ > 0 && activeDownloads_ >= maxConcurrentDownloads_) {
        resp->setStatusCode(503);
        resp->addHeader("Retry-After", "5");
        resp->setContentType("application/json");
        resp->setBody("{\"error\":\"too many concurrent downloads, try again later\"}");
        LOG_WARN("Download rejected: active downloads %d >= max %d",
                 activeDownloads_.load(), maxConcurrentDownloads_);
        return;
    }

    // 1. 在锁内查找元数据，拷贝 shared_ptr 后尽快解锁
    std::shared_ptr<FileMeta> meta;
    {
        std::lock_guard<std::mutex> lk(metaMutex_);
        auto it = codes_.find(code);
        if (it == codes_.end()) {
            resp->setStatusCode(404);
            resp->setContentType("application/json");
            resp->setBody("{\"error\":\"code not found\"}");
            return;
        }
        meta = it->second;  // 持有 shared_ptr，对象不会在解锁后被销毁
    }

    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // 2. 过期检查（文件 I/O 操作在锁外执行）
    if (meta->expireAt < nowMs) {
        ::unlink(meta->storagePath.c_str());
        auto lastSlash = meta->storagePath.rfind('/');
        if (lastSlash != std::string::npos) {
            std::string codeDir = meta->storagePath.substr(0, lastSlash);
            removeDir(codeDir);
        }
        {
            std::lock_guard<std::mutex> lk(metaMutex_);
            codes_.erase(code);
        }
        resp->setStatusCode(410); // Gone
        resp->setContentType("application/json");
        resp->setBody("{\"error\":\"file expired\"}");
        return;
    }

    // 3. 检查文件是否还存在（锁外 stat）
    struct stat st;
    if (::stat(meta->storagePath.c_str(), &st) != 0) {
        {
            std::lock_guard<std::mutex> lk(metaMutex_);
            codes_.erase(code);
        }
        resp->setStatusCode(404);
        resp->setContentType("application/json");
        resp->setBody("{\"error\":\"file missing from disk\"}");
        return;
    }

    // M3: 记录活跃下载（在异步 sendfile 之前）
    if (conn) {
        int fd = conn->fd();
        {
            std::lock_guard<std::mutex> lk(dlMapMutex_);
            activeDownloadSizes_[fd] = meta->fileSize;
        }
        activeDownloads_++;
    }

    // 4. 设置响应头并触发 sendfile
    resp->addHeader("Content-Disposition",
                    "attachment; filename=\"" + meta->originalName + "\"");
    resp->setFile(meta->storagePath);

    meta->downloadCount++;
    LOG_INFO("Download: code=%s name=%s size=%llu (active downloads: %d)",
             code.c_str(), meta->originalName.c_str(),
             static_cast<unsigned long long>(meta->fileSize),
             activeDownloads_.load());
}

// ============ 统计信息 ============

void EzdropServer::handleStats(HttpResponse* resp) {
    resp->setContentType("application/json");
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{"
             "\"totalBytesUploaded\":%llu,"
             "\"totalBytesDownloaded\":%llu,"
             "\"totalFilesUploaded\":%llu,"
             "\"totalFilesDownloaded\":%llu,"
             "\"activeDownloads\":%d"
             "}",
             static_cast<unsigned long long>(totalBytesUploaded_.load()),
             static_cast<unsigned long long>(totalBytesDownloaded_.load()),
             static_cast<unsigned long long>(totalFilesUploaded_.load()),
             static_cast<unsigned long long>(totalFilesDownloaded_.load()),
             activeDownloads_.load());
    resp->setBody(buf);
}

// ============ 写入完成回调（线程安全） ============

void EzdropServer::onWriteComplete(std::shared_ptr<Connection> conn) {
    if (!conn) return;
    int fd = conn->fd();

    std::lock_guard<std::mutex> lk(dlMapMutex_);
    auto it = activeDownloadSizes_.find(fd);
    if (it != activeDownloadSizes_.end()) {
        totalBytesDownloaded_ += it->second;
        totalFilesDownloaded_++;
        activeDownloadSizes_.erase(it);
        activeDownloads_--;
        LOG_DEBUG("Download complete for fd=%d, active downloads remaining: %d",
                  fd, activeDownloads_.load());
    }
}

// ============ 过期清理 ============

void EzdropServer::startExpirySweeper() {
    sweeperThread_ = std::thread([this]() {
        LOG_INFO("Expiry sweeper started");
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            if (!running_) break;
            sweepExpired();
        }
        LOG_INFO("Expiry sweeper stopped");
    });
}

void EzdropServer::sweepExpired() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // 1. 锁内收集过期条目的存储路径
    std::vector<std::string> expiredPaths;
    {
        std::lock_guard<std::mutex> lk(metaMutex_);
        for (auto it = codes_.begin(); it != codes_.end();) {
            if (it->second->expireAt < now) {
                expiredPaths.push_back(it->second->storagePath);
                it = codes_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 2. 锁外执行文件 I/O 删除
    for (const auto& path : expiredPaths) {
        ::unlink(path.c_str());
        auto lastSlash = path.rfind('/');
        if (lastSlash != std::string::npos) {
            std::string codeDir = path.substr(0, lastSlash);
            removeDir(codeDir);
        }
    }

    if (!expiredPaths.empty()) {
        LOG_INFO("Sweeper: removed %zu expired entries", expiredPaths.size());
    }
}

} // namespace ezNet
