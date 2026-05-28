#include "udp/CustomProtocol.h"
#include "util/Logger.h"
#include <cstring>

namespace ezNet {

void CustomProtocol::onMessage(const char* data, size_t len,
                               const struct sockaddr_in& addr,
                               const SendCallback& sender) {
    if (len < sizeof(MessageHeader)) return;
    auto* header = reinterpret_cast<const MessageHeader*>(data);
    if (header->magic != kMagicNumber) return;
    if (len < sizeof(MessageHeader) + header->length) return;

    const char* payload = data + sizeof(MessageHeader);
    size_t payloadLen = header->length;
    std::string res = "已收到：" + std::string(payload, payloadLen);
    sender(res.c_str(), res.size(), addr);
}

} // namespace ezNet
