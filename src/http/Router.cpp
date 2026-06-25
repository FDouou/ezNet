#include "http/Router.h"
#include <cctype>

namespace ezNet {

static std::string urlDecode(const std::string& src) {
    std::string result;
    result.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '%' && i + 2 < src.size()
            && std::isxdigit(static_cast<unsigned char>(src[i + 1]))
            && std::isxdigit(static_cast<unsigned char>(src[i + 2]))) {
            auto hexToVal = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return 0;
            };
            result += static_cast<char>(
                (hexToVal(src[i + 1]) << 4) | hexToVal(src[i + 2]));
            i += 2;
        } else if (src[i] == '+') {
            result += ' ';
        } else {
            result += src[i];
        }
    }
    return result;
}

static std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> segments;
    if (path.empty() || path[0] != '/') return segments;
    size_t start = 1;
    while (start < path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) {
            segments.push_back(path.substr(start));
            break;
        }
        if (end > start) {
            segments.push_back(path.substr(start, end - start));
        }
        start = end + 1;
    }
    return segments;
}

Router::RadixNode* Router::getOrCreateMethodRoot(const std::string& method) {
    auto it = methodRoots_.find(method);
    if (it != methodRoots_.end()) {
        return it->second.get();
    }
    auto node = std::make_unique<RadixNode>();
    auto* ptr = node.get();
    methodRoots_[method] = std::move(node);
    return ptr;
}

void Router::addRoute(const std::string& method, const std::string& path, Handler handler) {
    auto segments = splitPath(path);
    auto* node = getOrCreateMethodRoot(method);

    for (auto& seg : segments) {
        if (seg.empty()) continue;
        if (!seg.empty() && seg[0] == ':') {
            if (!node->paramChild) {
                node->paramChild = std::make_unique<RadixNode>();
                node->paramChild->segment = seg.substr(1);
            }
            node = node->paramChild.get();
        } else {
            auto it = node->children.find(seg);
            if (it == node->children.end()) {
                auto child = std::make_unique<RadixNode>();
                child->segment = seg;
                auto* ptr = child.get();
                node->children[seg] = std::move(child);
                node = ptr;
            } else {
                node = it->second.get();
            }
        }
    }
    node->handler = std::move(handler);
}

bool Router::route(HttpRequest& req, HttpResponse* resp,
                    std::shared_ptr<Connection> conn) const {
    auto methodIt = methodRoots_.find(req.methodRef());
    if (methodIt != methodRoots_.end()) {
        auto segments = splitPath(req.pathRef());
        req.pathParams_.clear();
        if (match(methodIt->second.get(), segments, 0, req, resp, conn)) {
            return true;
        }
    }
    if (defaultHandler_) {
        defaultHandler_(req, resp, conn);
        return true;
    }
    return false;
}

bool Router::match(RadixNode* node, const std::vector<std::string>& segments,
                   size_t index, HttpRequest& req, HttpResponse* resp,
                   std::shared_ptr<Connection> conn) const {
    if (index == segments.size()) {
        if (node->handler) {
            node->handler(req, resp, conn);
            return true;
        }
        return false;
    }

    const auto& seg = segments[index];

    auto childIt = node->children.find(seg);
    if (childIt != node->children.end()) {
        if (match(childIt->second.get(), segments, index + 1, req, resp, conn)) {
            return true;
        }
    }

    if (node->paramChild) {
        req.setPathParam(node->paramChild->segment, urlDecode(seg));
        if (match(node->paramChild.get(), segments, index + 1, req, resp, conn)) {
            return true;
        }
    }

    return false;
}

void Router::setDefaultHandler(Handler handler) {
    defaultHandler_ = std::move(handler);
}

} // namespace ezNet
