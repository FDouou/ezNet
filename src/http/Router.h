#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

namespace ezNet {

class Connection;

class Router {
public:
    using Handler = std::function<void(const HttpRequest& req, HttpResponse* resp,
                                        std::shared_ptr<Connection> conn)>;

    void addRoute(const std::string& method, const std::string& path, Handler handler);

    bool route(HttpRequest& req, HttpResponse* resp,
               std::shared_ptr<Connection> conn) const;

    void setDefaultHandler(Handler handler);

private:
    struct RadixNode {
        std::string segment;
        std::unordered_map<std::string, std::unique_ptr<RadixNode>> children;
        std::unique_ptr<RadixNode> paramChild;
        Handler handler;
    };

    RadixNode* getOrCreateMethodRoot(const std::string& method);
    bool match(RadixNode* node, const std::vector<std::string>& segments,
               size_t index, HttpRequest& req, HttpResponse* resp,
               std::shared_ptr<Connection> conn) const;

    std::unordered_map<std::string, std::unique_ptr<RadixNode>> methodRoots_;
    Handler defaultHandler_ = nullptr;
};

} // namespace ezNet
