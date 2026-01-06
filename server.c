#include "common.h"
#include "thread_pool.h"
#include "connection_pool.h"
#include <fcntl.h>

// ============================================
// 知识点标记：序号18 - 综合设计
// 多模块结合（线程池+聊天室+连接池）
// ============================================
// 全局变量
static thread_pool_t *g_thread_pool = NULL;
static connection_pool_t *g_connection_pool = NULL;
static int g_epoll_fd = -1;
static int g_running = 1;

// ============================================
// 知识点标记：序号2 - 多线程
// 代码中包含多线程部分（线程池工作函数）
// ============================================
// 处理客户端消息
void handle_client_message(void *arg) {
    message_t *msg = (message_t *)arg;
    int client_fd = msg->client_fd;
    char *buffer = msg->buffer;
    int len = msg->len;

    // 查找客户端信息（使用连接池）
    client_info_t *client = connection_pool_find(g_connection_pool, client_fd);
    if (client == NULL) {
        free(msg);
        return;
    }

    // 处理消息（简单回显并广播）
    buffer[len] = '\0';
    printf("[客户端 %s:%d] 消息: %s", client->ip, client->port, buffer);

    // 构造回复消息
    char reply[BUFFER_SIZE];
    snprintf(reply, sizeof(reply), "[服务器回复] 收到: %s", buffer);

    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // 序号13 - 连接池优化（考虑同步机制-线程安全设计）
    // 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
    // ============================================
    // 向所有连接的客户端广播消息
    pthread_mutex_lock(&(g_connection_pool->lock));
    connection_node_t *current = g_connection_pool->head;
    while (current != NULL) {
        if (current->client.active && current->client.fd != client_fd) {
            send(current->client.fd, reply, strlen(reply), 0);
        }
        current = current->next;
    }
    pthread_mutex_unlock(&(g_connection_pool->lock));

    // 向原客户端发送确认
    send(client_fd, reply, strlen(reply), 0);

    free(msg);
}

// 处理新的客户端连接
void handle_new_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd < 0) {
        perror("accept");
        return;
    }

    // ============================================
    // 知识点标记：序号4 - 网络编程（异步网络代码实现）
    // 序号16 - 高级特性（epoll实现高效I/O多路复用）
    // 设置非阻塞模式（epoll需要）
    // ============================================
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    // 创建客户端信息
    client_info_t client;
    client.fd = client_fd;
    inet_ntop(AF_INET, &(client_addr.sin_addr), client.ip, INET_ADDRSTRLEN);
    client.port = ntohs(client_addr.sin_port);
    client.connect_time = time(NULL);
    client.active = 1;

    // ============================================
    // 知识点标记：序号6 - 连接管理（连接池功能实现）
    // ============================================
    // 添加到连接池
    if (connection_pool_add(g_connection_pool, client) == 0) {
        printf("[新连接] %s:%d (连接池大小: %d)\n", 
               client.ip, client.port, 
               connection_pool_get_count(g_connection_pool));

        // ============================================
        // 知识点标记：序号16 - 高级特性
        // 使用epoll实现高效I/O多路复用（epoll_ctl添加监控）
        // ============================================
        // 添加到epoll
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET;  // 边缘触发模式
        event.data.fd = client_fd;
        epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
    } else {
        close(client_fd);
        printf("[连接被拒绝] 连接池已满\n");
    }
}

// 处理客户端数据
void handle_client_data(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

    if (n <= 0) {
        // 客户端断开连接
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            client_info_t *client = connection_pool_find(g_connection_pool, client_fd);
            if (client != NULL) {
                printf("[断开连接] %s:%d\n", client->ip, client->port);
            }
            
            // ============================================
            // 知识点标记：序号16 - 高级特性
            // 使用epoll实现高效I/O多路复用（epoll_ctl删除监控）
            // ============================================
            epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
            connection_pool_remove(g_connection_pool, client_fd);
            close(client_fd);
        }
        return;
    }

    // ============================================
    // 知识点标记：序号2 - 多线程
    // 序号9 - 多线程优化（实现线程池）
    // 创建消息任务并提交到线程池
    // ============================================
    message_t *msg = (message_t *)malloc(sizeof(message_t));
    if (msg != NULL) {
        msg->client_fd = client_fd;
        memcpy(msg->buffer, buffer, n);
        msg->len = n;
        thread_pool_add(g_thread_pool, handle_client_message, msg);
    }
}

// 信号处理函数
void signal_handler(int sig) {
    printf("\n[服务器] 收到信号 %d，正在关闭...\n", sig);
    g_running = 0;
}

int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr;
    struct epoll_event events[MAX_EVENTS];
    int port = 8888;

    // 解析命令行参数
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    // 设置socket选项（重用地址）
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(1);
    }

    // 监听
    if (listen(server_fd, 128) < 0) {
        perror("listen");
        close(server_fd);
        exit(1);
    }

    printf("[服务器] 启动成功，监听端口 %d\n", port);
    printf("[功能特性]\n");
    printf("  - 多线程服务器（线程池）\n");
    printf("  - epoll I/O多路复用\n");
    printf("  - 连接池管理\n");
    printf("  - 同步机制（互斥锁、条件变量）\n");
    printf("  - 线程安全设计\n");

    // ============================================
    // 知识点标记：序号16 - 高级特性
    // 使用epoll实现高效I/O多路复用（epoll_create1创建实例）
    // ============================================
    // 创建epoll实例
    g_epoll_fd = epoll_create1(0);
    if (g_epoll_fd < 0) {
        perror("epoll_create1");
        close(server_fd);
        exit(1);
    }

    // 将服务器socket添加到epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    // ============================================
    // 知识点标记：序号2 - 多线程
    // 序号9 - 多线程优化（实现线程池）
    // ============================================
    // 创建线程池
    g_thread_pool = thread_pool_create(THREAD_POOL_SIZE);
    if (g_thread_pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        close(server_fd);
        close(g_epoll_fd);
        exit(1);
    }
    printf("[线程池] 已创建 %d 个工作线程\n", THREAD_POOL_SIZE);

    // ============================================
    // 知识点标记：序号6 - 连接管理（连接池功能实现）
    // ============================================
    // 创建连接池
    g_connection_pool = connection_pool_create(CONNECTION_POOL_SIZE);
    if (g_connection_pool == NULL) {
        fprintf(stderr, "创建连接池失败\n");
        thread_pool_destroy(g_thread_pool);
        close(server_fd);
        close(g_epoll_fd);
        exit(1);
    }
    printf("[连接池] 最大连接数: %d\n", CONNECTION_POOL_SIZE);

    // ============================================
    // 知识点标记：序号4 - 网络编程（异步网络代码实现）
    // 序号16 - 高级特性（使用epoll实现高效I/O多路复用）
    // epoll_wait实现异步I/O多路复用
    // ============================================
    // 主事件循环（epoll实现高效I/O多路复用）
    printf("[服务器] 等待客户端连接...\n");
    while (g_running) {
        int nfds = epoll_wait(g_epoll_fd, events, MAX_EVENTS, 1000);

        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // 新的连接请求
                handle_new_connection(server_fd);
            } else {
                // 客户端数据到达
                handle_client_data(events[i].data.fd);
            }
        }
    }

    // ============================================
    // 知识点标记：序号18 - 综合设计
    // 多模块结合（线程池+聊天室+连接池）的清理
    // ============================================
    // 清理资源
    printf("[服务器] 正在关闭...\n");
    
    // 关闭所有客户端连接
    connection_pool_destroy(g_connection_pool);
    
    // 销毁线程池
    thread_pool_destroy(g_thread_pool);
    
    // 关闭epoll和服务器socket
    close(g_epoll_fd);
    close(server_fd);

    printf("[服务器] 已关闭\n");
    return 0;
}

