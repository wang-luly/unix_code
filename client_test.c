// 简单的测试客户端程序（可选）
#include "common.h"
#include <fcntl.h>
#include <sys/select.h>

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int port = 8888;
    const char *host = "localhost";

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }

    // 创建socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(1);
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        exit(1);
    }

    // 连接服务器
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        exit(1);
    }

    printf("已连接到服务器 %s:%d\n", host, port);
    printf("输入消息（输入'quit'退出）:\n");

    // 接收服务器消息（简单版本）
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        int max_fd = (sock_fd > STDIN_FILENO) ? sock_fd : STDIN_FILENO;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // 检查标准输入
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
                break;
            }

            if (strncmp(buffer, "quit", 4) == 0) {
                break;
            }

            send(sock_fd, buffer, strlen(buffer), 0);
        }

        // 检查socket
        if (FD_ISSET(sock_fd, &read_fds)) {
            ssize_t n = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);
            if (n <= 0) {
                printf("服务器断开连接\n");
                break;
            }
            buffer[n] = '\0';
            printf("[服务器] %s", buffer);
        }
    }

    close(sock_fd);
    return 0;
}

