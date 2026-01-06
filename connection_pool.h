#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include "common.h"

// ============================================
// 知识点标记：序号6 - 连接管理（连接池功能实现）
// ============================================

typedef struct connection_node {
    client_info_t client;
    struct connection_node *next;
} connection_node_t;

typedef struct connection_pool {
    connection_node_t *head;           // 连接链表头
    connection_node_t *tail;           // 连接链表尾
    int count;                         // 当前连接数
    int max_size;                      // 最大连接数
    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // 序号8 - 同步机制（使用条件变量）
    // 序号13 - 连接池优化（考虑同步机制-线程安全设计）
    // 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
    // ============================================
    pthread_mutex_t lock;              // 互斥锁（线程安全）
    pthread_cond_t not_empty;          // 条件变量：连接池非空
    pthread_cond_t not_full;           // 条件变量：连接池未满
} connection_pool_t;

// 函数声明
connection_pool_t *connection_pool_create(int max_size);
int connection_pool_add(connection_pool_t *pool, client_info_t client);
int connection_pool_remove(connection_pool_t *pool, int fd);
client_info_t *connection_pool_find(connection_pool_t *pool, int fd);
int connection_pool_get_count(connection_pool_t *pool);
void connection_pool_destroy(connection_pool_t *pool);

#endif // CONNECTION_POOL_H

