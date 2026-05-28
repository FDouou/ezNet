# ezNet - 高性能 C++ 网络框架

基于 epoll 的单线程 Reactor 非阻塞网络库，仅依赖 `http-parser`。

## 技术栈

- C++17
- CMake 3.16+
- Linux (epoll)
- http-parser (源码级嵌入)

## 目录结构

```
ezNet/
├── CMakeLists.txt
├── bench/           # 基准测试结果
├── src/
│   ├── core/         # 网络引擎层 (epoll, TCP, UDP, Buffer, Timer)
│   ├── http/         # HTTP 服务层 (解析、路由、响应)
│   ├── udp/          # UDP 应用层示例
│   ├── util/         # 工具类 (日志、配置)
│   └── main.cpp      # 入口
├── test/             # 单元测试
└── third_party/      # http-parser 源码
```

## 构建

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 运行

```bash
./ezNet
```

默认监听：

- HTTP: 8080
- UDP: 8081

## API 示例

### HTTP 服务器

```cpp
EventLoop loop;
TcpServer tcpServer(&loop, 8080);
HttpServer httpServer(&tcpServer);

httpServer.addRoute("GET", "/hello", [](auto& req, auto* resp) {
    resp->setContentType("text/plain");
    resp->setBody("Hello, World!");
});

httpServer.start();
loop.loop();
```

### UDP 回声服务

```cpp
EventLoop loop;
UdpServer udpServer(&loop, 8081);
UdpEcho echo;

udpServer.setMessageCallback([&](const char* data, size_t len, auto& addr) {
    udpServer.sendTo(data, len, addr);
});

udpServer.start();
loop.loop();
```

## 性能基准

> ⚠️ 以下数据为 **WSL2 虚拟环境下限**。WSL2 的系统调用需穿越 Hyper-V 虚拟化层，实际性能受此影响较大（syscall 密集型应用的常见瓶颈）。
> 原生 Linux 同等硬件下预期 **15-20 万 QPS**（见 [bench/bench.txt](bench/bench.txt) 压测环境说明）。

单线程 EventLoop，GCC 15，`wrk` 压测：

| 并发连接 | QPS | P50 延迟 | P99 延迟 |
|---|---|---|---|
| 1 (pipelining) | 21,000 | 38 μs | 275 μs |
| 10 | 75,576 | — | — |
| 100 | **90,450** | — | — |
| 100 (JSON) | 87,513 | 1.12 ms | 1.72 ms |
| 500 | 88,659 | 5.52 ms | 7.06 ms |

**关键趋势**（不受虚拟化影响）：

| 指标 | 表现 | 说明 |
|---|---|---|
| 峰值 QPS | ~9 万（WSL2 下限） | CPU 单核饱和后不再增长 |
| 延迟线性度 | 100→500 连接，延迟 1.1→5.5 ms | 单线程公平调度，无连接饿死 |
| 长尾延迟 | P99 与 P50 差距始终 < 2 ms | 无隐藏性能坑 |
| 内存 | 仅 8.2 MB RSS | 极低开销 |
| 稳定性 | 450 万请求零 FD 泄漏 | 架构可靠 |

> 瓶颈在单核 CPU，开启 `SO_REUSEPORT` + 多线程可线性扩展至全核利用。

