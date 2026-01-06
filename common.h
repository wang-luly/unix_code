#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#define MAX_EVENTS 1024
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 1000
#define THREAD_POOL_SIZE 10
#define CONNECTION_POOL_SIZE 50

// 客户端连接信息
typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    int port;
    time_t connect_time;
    int active;
} client_info_t;

// 消息结构
typedef struct {
    int client_fd;
    char buffer[BUFFER_SIZE];
    int len;
} message_t;

#endif // COMMON_H

