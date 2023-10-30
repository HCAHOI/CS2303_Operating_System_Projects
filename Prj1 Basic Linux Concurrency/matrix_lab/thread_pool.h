/*
*************************************************************************
This library can build a thread pool to run the tasks with multi threads.
*************************************************************************
 source file:
    thread_pool.h

 usage:
    import this library to your code, and call the functions. Use the function "pool_create()" to create a thread pool,
    and use the function "push_task()" to add a task to the thread pool. Use the function "pool_wait()" to wait for all
    the tasks to be finished
*/
#pragma once

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stddef.h>

struct thread_pool;
struct thread_pool_task;

typedef struct thread_pool pool_t;
typedef struct thread_pool_task task_t;
typedef void (*task_func_t)(void *);

struct thread_pool_task {
    void (*function)(void *);
    void *arg;
    struct thread_pool_task *next;
};

struct thread_pool {
    // task
    task_t *task_head;
    task_t *task_tail;
    // mutex
    pthread_mutex_t task_mutex;
    // condition
    pthread_cond_t work_cond;
    pthread_cond_t working_cond;
    // cnt
    size_t working_cnt;
    size_t thread_cnt;
    //bool
    bool shutdown;
};

//task functions
//create task
task_t *task_create(void (*function)(void *), void *arg) {
    if(function == NULL) {
        return NULL;
    }
    task_t *task = (task_t *)malloc(sizeof(task_t));
    task->function = function;
    task->arg = arg;
    task->next = NULL;
    return task;
}

//destroy task
void task_destroy(task_t *task) {
    if(task == NULL) {
        return;
    }
    free(task);
}

//pop task from the pool
task_t* task_pop(pool_t* m_pool) {
    if(m_pool == NULL) {
        return NULL;
    }

    task_t *task = m_pool->task_head;
    if(task == NULL) {
        return NULL;
    }

    if(task->next == NULL) {
        m_pool->task_head = NULL;
        m_pool->task_tail = NULL;
    } else {
        m_pool->task_head = task->next;
    }

    return task;
}

//create task and push to the pool
void task_push(pool_t* m_pool, task_func_t function, void *arg) {
    if(m_pool == NULL || function == NULL) {
        return;
    }

    task_t *task = task_create(function, arg);

    pthread_mutex_lock(&m_pool->task_mutex);
    if(m_pool->task_head == NULL) {
        m_pool->task_head = task;
        m_pool->task_tail = task;
    } else {
        m_pool->task_tail->next = task;
        m_pool->task_tail = task;
    }

    pthread_cond_broadcast(&m_pool->work_cond);
    pthread_mutex_unlock(&m_pool->task_mutex);
}

//run the task
void task_run(void* m_pool) {
    if (m_pool == NULL) {
        return;
    }

    pool_t *pool = (pool_t *) m_pool;
    task_t *task = NULL;

    while (1) {
        pthread_mutex_lock(&pool->task_mutex);

        while (pool->task_head == NULL) {
            pthread_cond_wait(&pool->work_cond, &pool->task_mutex);
        }

        if (pool->shutdown) {
            pool->thread_cnt--;
            pthread_cond_signal(&pool->working_cond);
            pthread_mutex_unlock(&pool->task_mutex);
            return;
        }

        task = task_pop(pool);
        pool->working_cnt++;
        pthread_mutex_unlock(&pool->task_mutex);

        if (task == NULL) {
            continue;
        }

        task->function(task->arg);
        task_destroy(task);

        pthread_mutex_lock(&pool->task_mutex);
        pthread_cond_signal(&pool->working_cond);
        pool->working_cnt--;
        pthread_mutex_unlock(&pool->task_mutex);
    }
}

//pool functions
//init the pool
pool_t* pool_create(size_t thread_num) {
    pool_t *m_pool = (pool_t *)malloc(sizeof(pool_t));

    //init
    m_pool->task_head = NULL;
    m_pool->task_tail = NULL;
    m_pool->working_cnt = 0;
    m_pool->thread_cnt = thread_num;
    m_pool->shutdown = false;
    pthread_mutex_init(&m_pool->task_mutex, NULL);
    pthread_cond_init(&m_pool->work_cond, NULL);
    pthread_cond_init(&m_pool->working_cond, NULL);

    //create threads
    for (int i = 0; i < thread_num; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, (void *)task_run, (void *)m_pool);
        pthread_detach(thread);
    }

    return m_pool;
}

//wait for all tasks to be completed
void pool_wait(pool_t* tm) {
    if (tm == NULL)
        return;

    pthread_mutex_lock(&tm->task_mutex);
    while (tm->task_head != NULL || tm->working_cnt != 0) {
            pthread_cond_wait(&tm->working_cond, &tm->task_mutex);
    }
    pthread_mutex_unlock(&tm->task_mutex);
}

//destroy the pool
void pool_destroy(pool_t* m_pool) {
    if (m_pool == NULL)
        return;

    pthread_mutex_lock(&m_pool->task_mutex);
    m_pool->shutdown = true;
    pthread_cond_broadcast(&m_pool->work_cond);
    pthread_mutex_unlock(&m_pool->task_mutex);

    pool_wait(m_pool);

    pthread_mutex_destroy(&m_pool->task_mutex);
    if(0 != m_pool->work_cond.__data.__wrefs) {
        m_pool->work_cond.__data.__wrefs = 0;
    }
    pthread_cond_destroy(&m_pool->work_cond);
    pthread_cond_destroy(&m_pool->working_cond);

    free(m_pool);
}
