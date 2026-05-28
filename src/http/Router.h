#pragma once

#include <functional>
#include <string>
#include <vector>
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

namespace ezNet {

class Router {
public:
    using Handler = std::function<void(const HttpRequest& req, HttpResponse* resp)>;

    void addRoute(const std::string& method, const std::string& path, Handler handler);

    bool route(const HttpRequest& req, HttpResponse* resp) const;

    void setDefaultHandler(Handler handler);

private:
    struct RouteEntry {
        std::string method;
        std::string path;
        Handler handler;
    };

    std::vector<RouteEntry> routes_ = {};
    Handler defaultHandler_ = nullptr;
};

} // namespace ezNet
