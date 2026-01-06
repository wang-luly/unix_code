#include "thread_pool.h"
#include <sys/syscall.h>

// ============================================
// 知识点标记：序号2 - 多线程（代码中包含多线程部分）
// 序号9 - 多线程优化（实现线程池）
// ============================================
// 线程池工作函数
static void *thread_pool_worker(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    task_t *task;

    while (1) {
        // ============================================
        // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
        // ============================================
        // 获取互斥锁
        pthread_mutex_lock(&(pool->lock));

        // ============================================
        // 知识点标记：序号8 - 同步机制（使用条件变量）
        // ============================================
        // 等待条件：任务队列非空或线程池关闭
        while ((pool->queue_size == 0) && (!pool->shutdown)) {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        // 检查是否关闭
        if (pool->shutdown) {
            break;
        }

        // 从任务队列取出任务
        task = pool->queue_head;
        if (task != NULL) {
            pool->queue_head = task->next;
            pool->queue_size--;

            if (pool->queue_head == NULL) {
                pool->queue_tail = NULL;
            }

            // 解锁
            pthread_mutex_unlock(&(pool->lock));

            // 执行任务
            (*(task->function))(task->arg);
            free(task);
        } else {
            pthread_mutex_unlock(&(pool->lock));
        }
    }

    pool->started--;
    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return NULL;
}

// ============================================
// 知识点标记：序号9 - 多线程优化（实现线程池）
// ============================================
// 创建线程池
thread_pool_t *thread_pool_create(int thread_count) {
    thread_pool_t *pool;
    int i;

    if (thread_count <= 0) {
        thread_count = THREAD_POOL_SIZE;
    }

    pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (pool == NULL) {
        return NULL;
    }

    // 初始化
    pool->thread_count = thread_count;
    pool->queue_size = 0;
    pool->shutdown = 0;
    pool->started = 0;
    pool->queue_head = NULL;
    pool->queue_tail = NULL;

    // 分配线程数组
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }

    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // 序号8 - 同步机制（使用条件变量）
    // ============================================
    // 初始化互斥锁和条件变量
    if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
        (pthread_cond_init(&(pool->notify), NULL) != 0)) {
        free(pool->threads);
        free(pool);
        return NULL;
    }

    // ============================================
    // 知识点标记：序号2 - 多线程（代码中包含多线程部分）
    // ============================================
    // 创建工作线程
    for (i = 0; i < thread_count; i++) {
        if (pthread_create(&(pool->threads[i]), NULL, thread_pool_worker, (void *)pool) != 0) {
            thread_pool_destroy(pool);
            return NULL;
        }
        pool->started++;
    }

    return pool;
}

// ============================================
// 知识点标记：序号9 - 多线程优化（实现线程池）
// ============================================
// 向线程池添加任务
int thread_pool_add(thread_pool_t *pool, void (*function)(void *), void *arg) {
    int err = 0;
    task_t *task;

    if (pool == NULL || function == NULL) {
        return -1;
    }

    // ============================================
    // 知识点标记：序号7 - 同步机制（使用锁-互斥锁）
    // ============================================
    if (pthread_mutex_lock(&(pool->lock)) != 0) {
        return -1;
    }

    if (pool->shutdown) {
        err = -1;
    } else {
        task = (task_t *)malloc(sizeof(task_t));
        if (task == NULL) {
            err = -1;
        } else {
            task->function = function;
            task->arg = arg;
            task->next = NULL;

            // 添加到队列尾部
            if (pool->queue_tail == NULL) {
                pool->queue_head = task;
                pool->queue_tail = task;
            } else {
                pool->queue_tail->next = task;
                pool->queue_tail = task;
            }
            pool->queue_size++;

            // ============================================
            // 知识点标记：序号8 - 同步机制（使用条件变量）
            // ============================================
            // 通知等待的线程
            pthread_cond_signal(&(pool->notify));
        }
    }

    if (pthread_mutex_unlock(&(pool->lock)) != 0) {
        err = -1;
    }

    return err;
}

// 销毁线程池
int thread_pool_destroy(thread_pool_t *pool) {
    int i, err = 0;

    if (pool == NULL) {
        return -1;
    }

    if (pthread_mutex_lock(&(pool->lock)) != 0) {
        return -1;
    }

    if (pool->shutdown) {
        err = -1;
    } else {
        pool->shutdown = 1;

        // 唤醒所有等待的线程
        if ((pthread_cond_broadcast(&(pool->notify)) != 0) ||
            (pthread_mutex_unlock(&(pool->lock)) != 0)) {
            err = -1;
        }

        // 等待所有线程退出
        for (i = 0; i < pool->thread_count; i++) {
            if (pthread_join(pool->threads[i], NULL) != 0) {
                err = -1;
            }
        }
    }

    if (!err) {
        thread_pool_t *temp_pool = pool;
        pool = NULL;
        // 清理任务队列
        task_t *task;
        while (temp_pool->queue_head != NULL) {
            task = temp_pool->queue_head;
            temp_pool->queue_head = temp_pool->queue_head->next;
            free(task);
        }
        pthread_mutex_destroy(&(temp_pool->lock));
        pthread_cond_destroy(&(temp_pool->notify));
        free(temp_pool->threads);
        free(temp_pool);
    }

    return err;
}

