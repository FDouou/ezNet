#include "http/HttpServer.h"
#include "util/Logger.h"
#include <cstring>

namespace ezNet {

HttpServer::HttpServer(TcpServer* tcpServer)
    : tcpServer_(tcpServer) {
    tcpServer_->setConnectionCallback(
        [this](auto conn){ onConnection(conn); }
    );
    tcpServer_->setDataCallback(
        [this](auto conn, auto buf){ onData(conn, buf); }
    );
}

HttpServer::~HttpServer() {
}

void HttpServer::addRoute(const std::string& method, const std::string& path, Handler handler) {
    router_.addRoute(method, path, handler);
}

void HttpServer::setConnectionHook(ConnectionHook hook) {
    connectionHook_ = std::move(hook);
}

void HttpServer::setWriteCompleteHook(WriteCompleteHook hook) {
    writeCompleteHook_ = std::move(hook);
}

void HttpServer::start() {
    tcpServer_->start();
}

void HttpServer::onConnection(std::shared_ptr<Connection> conn) {
    auto parserContext = std::make_shared<ParserContext>();
    
    // 初始化 http_parser
    http_parser_init(&parserContext->parser, HTTP_REQUEST);
    parserContext->parser.data = parserContext.get();
    
    // 初始化 http_parser_settings，注册所有回调
    http_parser_settings_init(&parserContext->settings);
    parserContext->settings.on_message_begin = onMessageBegin;
    parserContext->settings.on_url = onUrl;
    parserContext->settings.on_header_field = onHeaderField;
    parserContext->settings.on_header_value = onHeaderValue;
    parserContext->settings.on_headers_complete = onHeadersComplete;
    parserContext->settings.on_body = onBody;
    parserContext->settings.on_message_complete = onMessageComplete;
    
    conn->setUserData(parserContext.get());

    auto oldCb = conn->closeCallback();
    conn->setCloseCallback([parserContext, oldCb](std::shared_ptr<Connection> c) {
        c->setUserData(nullptr);
        if (oldCb) oldCb(c);
    });

    // 注册写入完成回调，用于文件响应发送完成后处理 keepAlive
    conn->setWriteCompleteCallback([this](std::shared_ptr<Connection> c) {
        // 先调用用户注册的钩子
        if (writeCompleteHook_) {
            writeCompleteHook_(c);
        }
        if (c->state() == Connection::State::SendingResponse) {
            // 文件发送已完成，处理 keepAlive
            if (c->isKeepAlive()) {
                c->resetForNextRequest();
            } else {
                c->close();
            }
        }
    });

    // 调用用户注册的连接钩子
    if (connectionHook_) {
        connectionHook_(conn);
    }
}

void HttpServer::onData(std::shared_ptr<Connection> conn, Buffer* buf) {
    auto parserContext = static_cast<ParserContext*>(conn->userData());
    if (!parserContext) return;

    if (conn->wheelEntry()) {
        tcpServer_->loop()->timeWheel().refresh(
            static_cast<TimeWheel::Entry*>(conn->wheelEntry()));
    }

    size_t nparsed = http_parser_execute(
        &parserContext->parser, 
        &parserContext->settings,
        buf->peek(),
        buf->readableBytes()
    );

    if (parserContext->parser.http_errno != HPE_OK) {
        HttpResponse resp;
        resp.setStatusCode(400);
        resp.setStatusMessage("Bad Request");
        conn->send(resp.build());
        conn->close();
        return;
    }

    buf->retrieve(nparsed);

    if (parserContext->messageComplete) {
        processRequest(conn);
        parserContext->messageComplete = false;
        parserContext->request.reset();
        http_parser_init(&parserContext->parser, HTTP_REQUEST);
    }
}

void HttpServer::processRequest(std::shared_ptr<Connection> conn) {
    auto parserContext = static_cast<ParserContext*>(conn->userData());
    if (!parserContext) return;

    HttpRequest& req = parserContext->request;
    HttpResponse resp;
    bool handled = router_.route(req, &resp, conn);
    if (!handled) {
        resp.setStatusCode(404);
        resp.setStatusMessage("Not Found");
        resp.setBody("404 Not Found");
    }
    resp.setKeepAlive(req.keepAlive());

    if (resp.isFile()) {
        if (req.hasRange()) {
            int64_t fileSize = static_cast<int64_t>(resp.fileSize());
            int64_t start = req.rangeStart();
            int64_t end = req.rangeEnd();

            // 处理 suffix 请求: bytes=-SUFFIX
            if (start == -1) {
                int64_t suffix = end;
                start = fileSize - suffix;
                end = fileSize - 1;
            }
            // 处理 START- 请求（到文件末尾）
            if (end == -1 || end >= fileSize) {
                end = fileSize - 1;
            }

            // 验证范围
            if (start < 0 || start >= fileSize || end < 0 || end >= fileSize || start > end) {
                // 416 Range Not Satisfiable
                HttpResponse errResp;
                errResp.setStatusCode(416);
                errResp.setStatusMessage("Range Not Satisfiable");
                errResp.addHeader("Content-Range", "bytes */" + std::to_string(fileSize));
                errResp.setKeepAlive(req.keepAlive());
                conn->send(errResp.build());
                if (!req.keepAlive()) {
                    conn->close();
                } else {
                    conn->resetForNextRequest();
                }
                return;
            }

            size_t sendLength = static_cast<size_t>(end - start + 1);

            // 构建 206 Partial Content 响应头
            HttpResponse rangeResp;
            rangeResp.setContentRange(start, end, fileSize);
            // 复制原响应的 Content-Type
            std::string contentType = resp.header("Content-Type");
            if (!contentType.empty()) {
                rangeResp.setContentType(contentType);
            }
            rangeResp.setKeepAlive(req.keepAlive());

            std::string header = rangeResp.build();
            conn->send(header);
            conn->sendFile(resp.filePath(), resp.fileSize(),
                           static_cast<off_t>(start), sendLength);
            conn->setState(Connection::State::SendingResponse);
        } else {
            // 普通文件响应：先发 HTTP 头，再 sendfile
            std::string header = resp.build();
            conn->send(header);
            conn->sendFile(resp.filePath(), resp.fileSize());
            // 文件响应不在此处处理 keepAlive，延迟到发送完成后的回调中处理
            conn->setState(Connection::State::SendingResponse);
        }
    } else {
        conn->send(resp.build());
        if (!req.keepAlive()) {
            conn->close();
        } else {
            conn->resetForNextRequest();
        }
    }
}

// http_parser 静态回调函数
int HttpServer::onMessageBegin(http_parser* parser) {
    auto parserContext = (ParserContext*)parser->data;
    parserContext->request.reset();
    return 0;
}

int HttpServer::onUrl(http_parser* parser, const char* at, size_t length) {
    auto parserContext = (ParserContext*)parser->data;
    parserContext->request.setUrl(at, length);
    return 0;
}

int HttpServer::onHeaderField(http_parser* parser, const char* at, size_t length) {
    auto parserContext = (ParserContext*)parser->data;
    parserContext->currentHeaderField.assign(at, length);
    return 0;
}

int HttpServer::onHeaderValue(http_parser* parser, const char* at, size_t length) {
    auto parserContext = (ParserContext*)parser->data;
    parserContext->request.setHeader(parserContext->currentHeaderField, std::string(at, length));
    return 0;
}

int HttpServer::onHeadersComplete(http_parser* parser) {
    auto parserContext = (ParserContext*)parser->data;
    parserContext->request.setMethod(http_method_str(static_cast<http_method>(parser->method)));
    parserContext->request.setHttpVersion(parser->http_major, parser->http_minor);
    parserContext->request.setKeepAlive(http_should_keep_alive(parser));
    parserContext->request.parseRange();
    return 0;
}

int HttpServer::onBody(http_parser* parser, const char* at, size_t length) {
    auto parserContext = (ParserContext*)parser->data;
    parserContext->request.appendBody(at, length);
    return 0;
}

int HttpServer::onMessageComplete(http_parser* parser) {
    auto parserContext = (ParserContext*)parser->data;
    parserContext->messageComplete = true;
    return 0;
}

} // namespace ezNet
