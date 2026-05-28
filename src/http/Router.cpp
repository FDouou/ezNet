#include "http/Router.h"

namespace ezNet {

void Router::addRoute(const std::string& method, const std::string& path, Handler handler) {
    routes_.push_back({method, path, handler});
}

bool Router::route(const HttpRequest& req, HttpResponse* resp) const {
    //遍历路由表找对应method和path
    auto method = req.methodRef();
    auto path = req.pathRef();
    for (auto& r : routes_) {
        if (r.method == method && r.path == path) {//找到，调用对应handler
            r.handler(req, resp);
            return true;
        }
    }
    if (defaultHandler_) {//没有找到，调用默认handler
        defaultHandler_(req, resp);
        return true;
    }
    return false;
}

void Router::setDefaultHandler(Handler handler) {
    defaultHandler_ = handler;
}

} // namespace ezNet
