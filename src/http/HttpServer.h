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
    using Handler = std::function<void(const HttpRequest& req, HttpResponse* resp)>;

    HttpServer(TcpServer* tcpServer);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void addRoute(const std::string& method, const std::string& path, Handler handler);

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
