#pragma once

#include <functional>
#include <string>
#include <netinet/in.h>

namespace ezNet {

class UdpEcho {
public:
    using SendCallback = std::function<void(const char*, size_t, const struct sockaddr_in&)>;
    
    void onMessage(const char* data, size_t len,
                   const struct sockaddr_in& addr,
                   const SendCallback& sender);
};

} // namespace ezNet
