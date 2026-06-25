# ezNet — 基于 epoll 的 C++17 网络框架

单线程 Reactor 模式，epoll ET（默认）/LT 可切换，非阻塞 IO。仅嵌入 http-parser

## 目录结构

```
ezNet/
├── CMakeLists.txt
├── bench/              # 基准测试结果（bench.txt）
├── examples/           # 示例应用
│   ├── ezdrop/         # 内网文件互传工具
│   └── image_hosting/  # 图床服务
├── src/
│   ├── core/           # EventLoop, TcpServer, UdpServer, Connection, Buffer, Timer, TimeWheel
│   ├── http/           # HttpServer, HttpRequest, HttpResponse, Router(Radix Tree)
│   ├── udp/            # UdpEcho, CustomProtocol
│   ├── util/           # Logger, Config, ThreadPool
│   └── main.cpp
├── test/               # 单元测试 + 集成测试
└── third_party/        # http-parser 源码
```

## 构建

- CMake 3.16+, C++17, Linux (epoll)
- 默认 Release：`-O3 -march=native -DNDEBUG`

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 运行

```bash
./ezNet
```

默认从 config.ini 读取配置；HTTP 8080 / UDP 8081。

## API 示例

### HTTP 服务器

```cpp
EventLoop loop;
TcpServer tcpServer(&loop, 8080);
HttpServer httpServer(&tcpServer);

httpServer.addRoute("GET", "/hello", [](const HttpRequest& req, HttpResponse* resp,
                                        const std::shared_ptr<Connection>&) {
    resp->setContentType("text/plain");
    resp->setBody("Hello, World!");
});

httpServer.addRoute("GET", "/users/:id", [](const HttpRequest& req, HttpResponse* resp,
                                            const std::shared_ptr<Connection>&) {
    resp->setContentType("application/json");
    resp->setBody("{\"id\":\"" + req.pathParam("id") + "\"}");
});

httpServer.start();
loop.loop();
```

### UDP 回声服务

```cpp
EventLoop loop;
UdpServer udpServer(&loop, 8081);

udpServer.setMessageCallback([&](const char* data, size_t len, const struct sockaddr_in& addr) {
    udpServer.sendTo(data, len, addr);
});

udpServer.start();
loop.loop();
```

## 示例应用

### ezdrop — 内网文件互传工具

服务端常驻提供 Web 界面，通过浏览器上传/下载文件。取件码为核心抽象：上传完成产出 6 位数字码，凭码下载。

- 单文件/多文件/目录上传（自动 tar.gz 打包）
- sendfile 零拷贝下载
- Range 断点续传
- 过期自动清理
- JSON 配置文件
- 最大文件大小限制（413）
- 并发下载限制（503 + Retry-After）
- 累计统计（GET /api/stats）

```bash
./ezdrop [-p port] [-d storage_dir] [-s static_dir] [-c config.json]
```

默认端口 8080。

### image_hosting — 图床服务

- POST /upload 上传图片
- GET /img/:filename 展示（sendfile 零拷贝）
- 内嵌 HTML 上传表单

```bash
./image_hosting [port] [storage_dir]
```

默认端口 8080，存储目录 /tmp/eznet_images。

## 性能基准

>  WSL2 环境 wrk测试

| 并发连接 | QPS | P50 延迟 | P99 延迟 |
|---|---|---|---|
| 100 | 174,661 | 557 μs | 0.92 ms |
| 500 | 165,472 | 2.98 ms | 4.12 ms |

## 测试

构建后在 `build/` 目录下运行各可执行测试文件。

- 单元测试：eventloop_test, buffer_test, tcp_server_test, udp_server_test, http_test, config_test, timewheel_test
- 集成测试：`test/ezdrop_integration_test.sh`（覆盖 ezdrop 全部功能）
