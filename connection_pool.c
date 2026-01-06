#include "connection_pool.h"

// ============================================
// 知识点标记：序号6 - 连接管理（连接池功能实现）
// ============================================
// 创建连接池
connection_pool_t *connection_pool_create(int max_size) {
    connection_pool_t *pool = (connection_pool_t *)malloc(sizeof(connection_pool_t));
    if (pool == NULL) {
        return NULL;
    }

    pool->head = NULL;
    pool->tail = NULL;
    pool->count = 0;
    pool->max_size = max_size;

    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // 序号8 - 同步机制（使用条件变量）
    // 序号13 - 连接池优化（考虑同步机制-线程安全设计）
    // ============================================
    // 初始化互斥锁和条件变量（线程安全）
    if (pthread_mutex_init(&(pool->lock), NULL) != 0) {
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&(pool->not_empty), NULL) != 0) {
        pthread_mutex_destroy(&(pool->lock));
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&(pool->not_full), NULL) != 0) {
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->not_empty));
        free(pool);
        return NULL;
    }

    return pool;
}

// ============================================
// 知识点标记：序号6 - 连接管理（连接池功能实现）
// 序号13 - 连接池优化（考虑同步机制-线程安全设计）
// 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
// ============================================
// 添加连接到连接池（考虑同步机制，避免竞态条件）
int connection_pool_add(connection_pool_t *pool, client_info_t client) {
    if (pool == NULL) {
        return -1;
    }

    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
    // ============================================
    pthread_mutex_lock(&(pool->lock));

    // 等待连接池未满
    while (pool->count >= pool->max_size) {
        pthread_cond_wait(&(pool->not_full), &(pool->lock));
    }

    connection_node_t *node = (connection_node_t *)malloc(sizeof(connection_node_t));
    if (node == NULL) {
        pthread_mutex_unlock(&(pool->lock));
        return -1;
    }

    node->client = client;
    node->client.active = 1;
    node->next = NULL;

    if (pool->tail == NULL) {
        pool->head = node;
        pool->tail = node;
    } else {
        pool->tail->next = node;
        pool->tail = node;
    }

    pool->count++;

    // ============================================
    // 知识点标记：序号8 - 同步机制（使用条件变量）
    // ============================================
    // 通知等待的线程：连接池非空
    pthread_cond_signal(&(pool->not_empty));
    pthread_mutex_unlock(&(pool->lock));

    return 0;
}

// ============================================
// 知识点标记：序号6 - 连接管理（连接池功能实现）
// 序号13 - 连接池优化（考虑同步机制-线程安全设计）
// 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
// ============================================
// 从连接池移除连接
int connection_pool_remove(connection_pool_t *pool, int fd) {
    if (pool == NULL) {
        return -1;
    }

    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
    // ============================================
    pthread_mutex_lock(&(pool->lock));

    connection_node_t *current = pool->head;
    connection_node_t *prev = NULL;

    while (current != NULL) {
        if (current->client.fd == fd) {
            if (prev == NULL) {
                pool->head = current->next;
            } else {
                prev->next = current->next;
            }

            if (current == pool->tail) {
                pool->tail = prev;
            }

            free(current);
            pool->count--;

            // ============================================
            // 知识点标记：序号8 - 同步机制（使用条件变量）
            // ============================================
            // 通知等待的线程：连接池未满
            pthread_cond_signal(&(pool->not_full));
            pthread_mutex_unlock(&(pool->lock));
            return 0;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&(pool->lock));
    return -1;
}

// ============================================
// 知识点标记：序号6 - 连接管理（连接池功能实现）
// 序号13 - 连接池优化（考虑同步机制-线程安全设计）
// 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
// ============================================
// 查找连接
client_info_t *connection_pool_find(connection_pool_t *pool, int fd) {
    if (pool == NULL) {
        return NULL;
    }

    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
    // ============================================
    pthread_mutex_lock(&(pool->lock));

    connection_node_t *current = pool->head;
    while (current != NULL) {
        if (current->client.fd == fd && current->client.active) {
            pthread_mutex_unlock(&(pool->lock));
            return &(current->client);
        }
        current = current->next;
    }

    pthread_mutex_unlock(&(pool->lock));
    return NULL;
}

// ============================================
// 知识点标记：序号6 - 连接管理（连接池功能实现）
// 序号13 - 连接池优化（考虑同步机制-线程安全设计）
// 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
// ============================================
// 获取连接数
int connection_pool_get_count(connection_pool_t *pool) {
    int count;
    if (pool == NULL) {
        return -1;
    }

    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // 序号14 - 连接池优化（考虑互斥机制-避免竞态条件）
    // ============================================
    pthread_mutex_lock(&(pool->lock));
    count = pool->count;
    pthread_mutex_unlock(&(pool->lock));

    return count;
}

// 销毁连接池
void connection_pool_destroy(connection_pool_t *pool) {
    if (pool == NULL) {
        return;
    }

    pthread_mutex_lock(&(pool->lock));

    connection_node_t *current = pool->head;
    while (current != NULL) {
        connection_node_t *next = current->next;
        close(current->client.fd);
        free(current);
        current = next;
    }

    pthread_mutex_unlock(&(pool->lock));
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->not_empty));
    pthread_cond_destroy(&(pool->not_full));
    free(pool);
}

