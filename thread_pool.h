#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "common.h"

// ============================================
// 知识点标记：序号9 - 多线程优化（实现线程池）
// ============================================

typedef struct task {
    void (*function)(void *arg);
    void *arg;
    struct task *next;
} task_t;

typedef struct thread_pool {
    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // 序号8 - 同步机制（使用条件变量）
    // ============================================
    pthread_mutex_t lock;          // 互斥锁
    pthread_cond_t notify;         // 条件变量
    pthread_t *threads;            // 线程数组
    task_t *queue_head;            // 任务队列头
    task_t *queue_tail;            // 任务队列尾
    int thread_count;              // 线程数量
    int queue_size;                // 当前任务数量
    int shutdown;                  // 关闭标志
    int started;                   // 启动的线程数
} thread_pool_t;

// 函数声明
thread_pool_t *thread_pool_create(int thread_count);
int thread_pool_add(thread_pool_t *pool, void (*function)(void *), void *arg);
int thread_pool_destroy(thread_pool_t *pool);

#endif // THREAD_POOL_H

