#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/Router.h"
#include <cassert>
#include <cstring>
#include <iostream>

using namespace ezNet;

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; g_fail++; } \
    else { g_pass++; } \
} while(0)

void test_http_request_default() {
    HttpRequest req;
    EXPECT(req.methodRef().empty(), "default: method empty");
    EXPECT(req.urlRef().empty(), "default: url empty");
    EXPECT(req.body().empty(), "default: body empty");
    EXPECT(!req.keepAlive(), "default: keepAlive=false");
    EXPECT(req.httpMajor() == 1, "default: httpMajor=1");
    EXPECT(req.httpMinor() == 1, "default: httpMinor=1");
}

void test_http_request_set_method() {
    HttpRequest req;
    req.setMethod("POST");
    EXPECT(req.methodRef() == "POST", "setMethod: returns correct value");
    EXPECT(req.method() == "POST", "method() returns copy");
}

void test_http_request_set_url() {
    HttpRequest req;
    req.setUrl("/api/users?page=1");
    EXPECT(req.urlRef() == "/api/users?page=1", "setUrl: full url stored");
    EXPECT(req.pathRef() == "/api/users", "setUrl: path extracted");
    EXPECT(req.queryStringRef() == "page=1", "setUrl: queryString extracted");
}

void test_http_request_set_url_no_query() {
    HttpRequest req;
    req.setUrl("/index.html");
    EXPECT(req.urlRef() == "/index.html", "setUrl no query: url stored");
    EXPECT(req.pathRef() == "/index.html", "setUrl no query: path=url");
    EXPECT(req.queryStringRef().empty(), "setUrl no query: queryString empty");
}

void test_http_request_set_url_raw() {
    HttpRequest req;
    req.setUrl("/hello", 6);
    EXPECT(req.pathRef() == "/hello", "setUrl raw: path correct");
}

void test_http_request_headers() {
    HttpRequest req;
    req.setHeader("Host", "localhost");
    req.setHeader("Content-Type", "text/plain");
    EXPECT(req.header("Host") == "localhost", "setHeader: Host correct");
    EXPECT(req.header("Content-Type") == "text/plain", "setHeader: Content-Type correct");
    const auto& hdrs = req.headers();
    EXPECT(hdrs.size() == 2, "headers: count correct");
}

void test_http_request_header_case_insensitive() {
    HttpRequest req;
    req.setHeader("Content-Type", "application/json");
    EXPECT(req.header("content-type") == "application/json", "header: case insensitive lookup");
    EXPECT(req.header("CONTENT-TYPE") == "application/json", "header: uppercase lookup");
    EXPECT(req.header("Content-type") == "application/json", "header: mixed case lookup");
}

void test_http_request_header_missing() {
    HttpRequest req;
    EXPECT(req.header("X-Missing").empty(), "header: missing returns empty");
}

void test_http_request_body() {
    HttpRequest req;
    req.setBody("hello body");
    EXPECT(req.body() == "hello body", "setBody: correct");
}

void test_http_request_append_body() {
    HttpRequest req;
    req.appendBody("abc", 3);
    req.appendBody("def", 3);
    EXPECT(req.body() == "abcdef", "appendBody: concatenated correctly");
}

void test_http_request_keep_alive() {
    HttpRequest req;
    req.setKeepAlive(true);
    EXPECT(req.keepAlive(), "setKeepAlive: true");
    req.setKeepAlive(false);
    EXPECT(!req.keepAlive(), "setKeepAlive: false");
}

void test_http_request_http_version() {
    HttpRequest req;
    req.setHttpVersion(2, 0);
    EXPECT(req.httpMajor() == 2, "setHttpVersion: major correct");
    EXPECT(req.httpMinor() == 0, "setHttpVersion: minor correct");
}

void test_http_request_reset() {
    HttpRequest req;
    req.setMethod("GET");
    req.setUrl("/test");
    req.setKeepAlive(true);
    req.setBody("data");
    req.setHttpVersion(2, 0);
    req.reset();
    EXPECT(req.methodRef().empty(), "reset: method cleared");
    EXPECT(req.urlRef().empty(), "reset: url cleared");
    EXPECT(req.body().empty(), "reset: body cleared");
    EXPECT(!req.keepAlive(), "reset: keepAlive false");
    EXPECT(req.httpMajor() == 1, "reset: httpMajor reset to 1");
}

void test_http_response_default() {
    HttpResponse resp;
    EXPECT(resp.statusCode() == 200, "default: statusCode=200");
    EXPECT(resp.statusMessage() == "OK", "default: statusMessage=OK");
    EXPECT(!resp.keepAlive(), "default: keepAlive=false");
    EXPECT(!resp.isChunked(), "default: chunked=false");
}

void test_http_response_build_basic() {
    HttpResponse resp;
    resp.setBody("Hello");
    std::string result = resp.build();
    EXPECT(result.find("HTTP/1.1 200 OK") != std::string::npos, "build: status line present");
    EXPECT(result.find("Server: ezNet") != std::string::npos, "build: Server header present");
    EXPECT(result.find("Content-Length: 5") != std::string::npos, "build: Content-Length present");
    EXPECT(result.find("Hello") != std::string::npos, "build: body present");
}

void test_http_response_build_404() {
    HttpResponse resp;
    resp.setStatusCode(404);
    resp.setStatusMessage("Not Found");
    resp.setBody("Page not found");
    std::string result = resp.build();
    EXPECT(result.find("HTTP/1.1 404 Not Found") != std::string::npos, "build: 404 status line");
}

void test_http_response_set_body_raw() {
    HttpResponse resp;
    const char* data = "binary";
    resp.setBody(data, 6);
    EXPECT(resp.body() == "binary", "setBody raw: content correct");
}

void test_http_response_set_content_type() {
    HttpResponse resp;
    resp.setContentType("application/json");
    std::string result = resp.build();
    EXPECT(result.find("Content-Type: application/json") != std::string::npos, "build: Content-Type header");
}

void test_http_response_chunked() {
    HttpResponse resp;
    resp.setChunked(true);
    resp.setBody("chunked body");
    std::string result = resp.build();
    EXPECT(result.find("Transfer-Encoding: chunked") != std::string::npos, "build: chunked header present");
}

void test_http_response_add_header() {
    HttpResponse resp;
    resp.addHeader("X-Custom", "value");
    std::string result = resp.build();
    EXPECT(result.find("X-Custom: value") != std::string::npos, "build: custom header present");
}

void test_http_response_keep_alive() {
    HttpResponse resp;
    resp.setKeepAlive(true);
    EXPECT(resp.keepAlive(), "setKeepAlive: true");
    resp.setKeepAlive(false);
    EXPECT(!resp.keepAlive(), "setKeepAlive: false");
}

void test_http_response_reset() {
    HttpResponse resp;
    resp.setStatusCode(500);
    resp.setBody("error");
    resp.setChunked(true);
    resp.setKeepAlive(true);
    resp.reset();
    EXPECT(resp.statusCode() == 200, "reset: statusCode=200");
    EXPECT(resp.statusMessage() == "OK", "reset: statusMessage=OK");
    EXPECT(resp.body().empty(), "reset: body empty");
    EXPECT(!resp.keepAlive(), "reset: keepAlive=false");
    EXPECT(!resp.isChunked(), "reset: chunked=false");
}

void test_router_add_and_route() {
    Router router;
    bool called = false;
    router.addRoute("GET", "/hello", [&](const HttpRequest& req, HttpResponse* resp) {
        called = true;
        resp->setBody("hello");
    });

    HttpRequest req;
    req.setMethod("GET");
    req.setUrl("/hello");
    HttpResponse resp;
    bool handled = router.route(req, &resp);
    EXPECT(handled, "route: matched handler returns true");
    EXPECT(called, "route: handler was called");
}

void test_router_no_match() {
    Router router;
    router.addRoute("GET", "/hello", [](const HttpRequest&, HttpResponse*) {});

    HttpRequest req;
    req.setMethod("GET");
    req.setUrl("/unknown");
    HttpResponse resp;
    bool handled = router.route(req, &resp);
    EXPECT(!handled, "route: no match returns false");
}

void test_router_method_mismatch() {
    Router router;
    router.addRoute("GET", "/hello", [](const HttpRequest&, HttpResponse*) {});

    HttpRequest req;
    req.setMethod("POST");
    req.setUrl("/hello");
    HttpResponse resp;
    bool handled = router.route(req, &resp);
    EXPECT(!handled, "route: method mismatch returns false");
}

void test_router_default_handler() {
    Router router;
    bool defaultCalled = false;
    router.setDefaultHandler([&](const HttpRequest& req, HttpResponse* resp) {
        defaultCalled = true;
        resp->setStatusCode(404);
    });

    HttpRequest req;
    req.setMethod("GET");
    req.setUrl("/noexist");
    HttpResponse resp;
    bool handled = router.route(req, &resp);
    EXPECT(handled, "route: default handler returns true");
    EXPECT(defaultCalled, "route: default handler called");
    EXPECT(resp.statusCode() == 404, "route: default handler set 404");
}

void test_router_multiple_routes() {
    Router router;
    router.addRoute("GET", "/a", [](const HttpRequest&, HttpResponse* resp) { resp->setBody("A"); });
    router.addRoute("GET", "/b", [](const HttpRequest&, HttpResponse* resp) { resp->setBody("B"); });
    router.addRoute("POST", "/a", [](const HttpRequest&, HttpResponse* resp) { resp->setBody("PA"); });

    HttpRequest req;
    HttpResponse resp;

    req.setMethod("GET"); req.setUrl("/a");
    router.route(req, &resp);
    EXPECT(resp.body() == "A", "route: GET /a -> A");

    resp.reset();
    req.setMethod("GET"); req.setUrl("/b");
    router.route(req, &resp);
    EXPECT(resp.body() == "B", "route: GET /b -> B");

    resp.reset();
    req.setMethod("POST"); req.setUrl("/a");
    router.route(req, &resp);
    EXPECT(resp.body() == "PA", "route: POST /a -> PA");
}

void test_router_default_always_returns_true() {
    Router router;
    router.setDefaultHandler([](const HttpRequest&, HttpResponse*) {});

    HttpRequest req;
    req.setMethod("ANY"); req.setUrl("/any");
    HttpResponse resp;
    EXPECT(router.route(req, &resp), "router with default handler always returns true");
}

int main() {
    std::cout << "=== HTTP Tests ===" << std::endl;

    test_http_request_default();
    test_http_request_set_method();
    test_http_request_set_url();
    test_http_request_set_url_no_query();
    test_http_request_set_url_raw();
    test_http_request_headers();
    test_http_request_header_case_insensitive();
    test_http_request_header_missing();
    test_http_request_body();
    test_http_request_append_body();
    test_http_request_keep_alive();
    test_http_request_http_version();
    test_http_request_reset();
    test_http_response_default();
    test_http_response_build_basic();
    test_http_response_build_404();
    test_http_response_set_body_raw();
    test_http_response_set_content_type();
    test_http_response_chunked();
    test_http_response_add_header();
    test_http_response_keep_alive();
    test_http_response_reset();
    test_router_add_and_route();
    test_router_no_match();
    test_router_method_mismatch();
    test_router_default_handler();
    test_router_multiple_routes();
    test_router_default_always_returns_true();

    std::cout << "Passed: " << g_pass << ", Failed: " << g_fail << std::endl;
    return g_fail > 0 ? 1 : 0;
}
