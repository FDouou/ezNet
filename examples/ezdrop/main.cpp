#include "EzdropServer.h"
#include "core/EventLoop.h"
#include "core/TcpServer.h"
#include "http/HttpServer.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "util/Logger.h"
#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <ifaddrs.h>
#include <iostream>
#include <net/if.h>
#include <string>

using namespace ezNet;

static EventLoop* gLoop = nullptr;

static void signalHandler(int) {
    LOG_INFO("ezdrop shutting down...");
    if (gLoop) gLoop->stop();
}

/// 获取可执行文件所在目录
static std::string getExeDir(const char* argv0) {
    std::string path(argv0);
    // 尝试解析为绝对路径
    char* absPath = realpath(path.c_str(), nullptr);
    if (absPath) {
        path = absPath;
        free(absPath);
    }
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return ".";
}

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " [-p port] [-d storage_dir] [-s static_dir] [-c config.json]\n"
              << "  -p   listen port (default 8080)\n"
              << "  -d   storage directory (default ./data)\n"
              << "  -s   static files directory (default: derived from executable path)\n"
              << "  -c   JSON configuration file (CLI args override JSON values)\n";
}

static std::string getLocalIP() {
    struct ifaddrs* ifaddr = nullptr;
    std::string ip = "127.0.0.1";
    if (getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            // 跳过 loopback
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;
            char buf[INET_ADDRSTRLEN];
            auto* addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
            ip = buf;
            break;
        }
        freeifaddrs(ifaddr);
    }
    return ip;
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    std::string storageDir = "./data";
    std::string staticDir;  // 空字符串表示使用默认路径（由构造函数处理）
    std::string configPath;
    bool cliPort = false;
    bool cliStorageDir = false;

    // 第一遍：解析 CLI 参数
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-p" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
            cliPort = true;
        } else if (a == "-d" && i + 1 < argc) {
            storageDir = argv[++i];
            cliStorageDir = true;
        } else if (a == "-s" && i + 1 < argc) {
            staticDir = argv[++i];
        } else if (a == "-c" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (a == "-h" || a == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            printUsage(argv[0]);
            return 1;
        }
    }

    // 如果未指定静态目录，从可执行文件路径推导
    if (staticDir.empty()) {
        staticDir = getExeDir(argv[0]) + "/static";
    }

    // 第二遍：如果指定了配置文件，加载配置（优先级：JSON < CLI）
    if (!configPath.empty()) {
        EzdropConfig cfg = EzdropServer::loadConfigFile(configPath);
        // 先应用 JSON 中的值
        if (!cliPort) {
            port = cfg.port;
        }
        if (!cliStorageDir) {
            storageDir = cfg.storageDir;
        }
        // 保存运行时参数待后续应用
        // maxFileSize, maxConcurrentDownloads 在创建服务器后通过 loadConfig 应用
        LOG_INFO("Config file: %s port=%u storage=%s maxFileSizeMb=%zu maxConcurrent=%d",
                 configPath.c_str(), cfg.port, cfg.storageDir.c_str(),
                 cfg.maxFileSizeMb, cfg.maxConcurrentDownloads);
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    EventLoop loop;
    gLoop = &loop;

    TcpServer tcpServer(&loop, port);
    HttpServer httpServer(&tcpServer);
    EzdropServer ezdrop(&loop, storageDir, port, staticDir);

    // 应用配置文件中的运行时参数（在服务器创建后设置）
    if (!configPath.empty()) {
        ezdrop.loadConfig(configPath);
    }

    // Wire up write complete hook for tracking download completion
    httpServer.setWriteCompleteHook([&](std::shared_ptr<Connection> conn) {
        ezdrop.onWriteComplete(conn);
    });

    // 首页
    httpServer.addRoute("GET", "/", [&](const HttpRequest&, HttpResponse* resp,
                                         std::shared_ptr<Connection>) {
        ezdrop.handleIndex(resp);
    });

    // 上传
    httpServer.addRoute("POST", "/upload", [&](const HttpRequest& req, HttpResponse* resp,
                                                std::shared_ptr<Connection>) {
        ezdrop.handleUpload(req, resp);
    });

    // 查询
    httpServer.addRoute("GET", "/api/meta/:code", [&](const HttpRequest& req, HttpResponse* resp,
                                                       std::shared_ptr<Connection>) {
        ezdrop.handleQuery(req, resp);
    });

    // 统计
    httpServer.addRoute("GET", "/api/stats", [&](const HttpRequest&, HttpResponse* resp,
                                                  std::shared_ptr<Connection>) {
        ezdrop.handleStats(resp);
    });

    // 下载——使用 HttpResponse::setFile() 触发零拷贝 sendfile
    httpServer.addRoute("GET", "/d/:code", [&](const HttpRequest& req, HttpResponse* resp,
                                                std::shared_ptr<Connection> conn) {
        ezdrop.handleDownload(req, resp, conn);
    });

    tcpServer.start();
    ezdrop.startExpirySweeper();
    LOG_INFO("ezdrop serving on http://%s:%d (storage: %s)",
             getLocalIP().c_str(), port, storageDir.c_str());
    loop.loop();

    return 0;
}
