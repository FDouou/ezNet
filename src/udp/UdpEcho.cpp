#include "udp/UdpEcho.h"
#include <cstring>

namespace ezNet {

void UdpEcho::onMessage(const char* data, size_t len,
                        const struct sockaddr_in& addr,
                        const SendCallback& sender) {
    sender(data, len, addr);
}

} // namespace ezNet
