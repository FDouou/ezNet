#include "core/EventLoop.h"
#include "core/TcpServer.h"
#include "core/UdpServer.h"
#include "http/HttpServer.h"
#include "http/HttpResponse.h"
#include "udp/UdpEcho.h"
#include "util/Logger.h"
#include "util/Config.h"

using namespace ezNet;

int main() {
    LOG_INFO("ezNet starting...");

    EventLoop loop;

    Config config("config.ini");

    TcpServer httpTcp(&loop, config.httpPort);
    HttpServer httpService(&httpTcp);

    httpService.addRoute("GET", "/hello", [](const HttpRequest& req, HttpResponse* resp,
                                              std::shared_ptr<Connection>) {
        resp->setContentType("text/plain");
        resp->setBody("Hello, ezNet!");
    });

    httpService.addRoute("GET", "/status", [](const HttpRequest& req, HttpResponse* resp,
                                               std::shared_ptr<Connection>) {
        resp->setContentType("application/json");
        resp->setBody("{\"status\":\"ok\"}");
    });

    httpService.start();

    UdpServer udpServer(&loop, config.udpPort);
    UdpEcho echo;

    udpServer.setMessageCallback([&echo, &udpServer](const char* data, size_t len,
                                                       const struct sockaddr_in& addr) {
        echo.onMessage(data, len, addr,
            [&udpServer](const char* d, size_t n, const struct sockaddr_in& a) {
                udpServer.sendTo(d, n, a);
            });
    });

    udpServer.start();

    LOG_INFO("ezNet server started on HTTP:%u UDP:%u", config.httpPort, config.udpPort);

    loop.loop();

    LOG_INFO("ezNet server stopped");
    return 0;
}
