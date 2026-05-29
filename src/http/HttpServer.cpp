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
    
    HttpResponse resp;
    bool handled = router_.route(parserContext->request, &resp);
    if (!handled) {
        resp.setStatusCode(404);
        resp.setStatusMessage("Not Found");
        resp.setBody("404 Not Found");
    }
    resp.setKeepAlive(parserContext->request.keepAlive());
    conn->send(resp.build());
    if (!parserContext->request.keepAlive()) {
        conn->close();
    } else {
        conn->resetForNextRequest();
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
