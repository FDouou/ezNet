#pragma once

#include <functional>
#include <memory>
#include <string>
#include "core/TcpServer.h"
#include "core/Connection.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/Router.h"
#include "http_parser.h"

namespace ezNet {

class HttpServer {
public:
    using Handler = std::function<void(const HttpRequest& req, HttpResponse* resp,
                                        std::shared_ptr<Connection> conn)>;
    using ConnectionHook = std::function<void(std::shared_ptr<Connection>)>;
    using WriteCompleteHook = std::function<void(std::shared_ptr<Connection>)>;

    HttpServer(TcpServer* tcpServer);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void addRoute(const std::string& method, const std::string& path, Handler handler);

    /// 注册连接建立时的钩子（在默认 onConnection 之后调用）
    void setConnectionHook(ConnectionHook hook);

    /// 注册写入完成时的钩子（在默认 writeCompleteCallback 之前调用）
    void setWriteCompleteHook(WriteCompleteHook hook);

    void start();

private:
    void onConnection(std::shared_ptr<Connection> conn);
    void onData(std::shared_ptr<Connection> conn, Buffer* buf);
    void processRequest(std::shared_ptr<Connection> conn);

    static int onMessageBegin(http_parser* parser);
    static int onUrl(http_parser* parser, const char* at, size_t length);
    static int onHeaderField(http_parser* parser, const char* at, size_t length);
    static int onHeaderValue(http_parser* parser, const char* at, size_t length);
    static int onHeadersComplete(http_parser* parser);
    static int onBody(http_parser* parser, const char* at, size_t length);
    static int onMessageComplete(http_parser* parser);

    TcpServer* tcpServer_;
    Router router_;

    ConnectionHook connectionHook_;
    WriteCompleteHook writeCompleteHook_;

    struct ParserContext {
        http_parser parser;
        http_parser_settings settings;
        HttpRequest request;
        std::string currentHeaderField;
        bool headersComplete = false;
        bool messageComplete = false;
    };
};

} // namespace ezNet
