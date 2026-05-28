#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char* argv[]) {

    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? std::atoi(argv[2]) : 8082;
    const char* msg = argc > 3 ? argv[3] : "Hello from test client!";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("Connected to %s:%d, sending: %s\n", host, port, msg);

    ssize_t sent = send(sock, msg, strlen(msg), 0);
    if (sent < 0) {
        perror("send");
        close(sock);
        return 1;
    }

    char buf[4096];
    ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        printf("Received (%zd bytes): %s\n", n, buf);
    } else {
        perror("recv");
    }

    close(sock);
    return 0;
}
