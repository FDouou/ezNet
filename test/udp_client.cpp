#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    //TODO:
    // 1. 解析命令行参数: host port message
    // 2. 创建 UDP socket
    // 3. 发送数据报
    // 4. 接收并打印响应

    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? std::atoi(argv[2]) : 8081;
    const char* msg = argc > 3 ? argv[3] : "Hello UDP!";

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    ssize_t sent = sendto(sock, msg, strlen(msg), 0,
                          (struct sockaddr*)&addr, sizeof(addr));
    if (sent < 0) {
        perror("sendto");
        close(sock);
        return 1;
    }
    printf("Sent %zd bytes to %s:%d\n", sent, host, port);

    char buf[4096];
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                         (struct sockaddr*)&fromAddr, &fromLen);
    if (n > 0) {
        buf[n] = '\0';
        printf("Received (%zd bytes): %s\n", n, buf);
    } else {
        perror("recvfrom");
    }

    close(sock);
    return 0;
}
