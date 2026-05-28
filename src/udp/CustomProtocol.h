#pragma once

#include <functional>
#include <string>
#include <vector>
#include <netinet/in.h>

namespace ezNet {

class CustomProtocol {
public:
    using SendCallback = std::function<void(const char*, size_t, const struct sockaddr_in&)>;

    void onMessage(const char* data, size_t len,
                   const struct sockaddr_in& addr,
                   const SendCallback& sender);

private:
    struct MessageHeader {
        uint32_t magic;
        uint32_t length;
        uint32_t type;
    };
    static constexpr uint32_t kMagicNumber = 0xFFDD0011;
};

} // namespace ezNet
