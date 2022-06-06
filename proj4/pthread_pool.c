/*
 * Copyright 2022. Heekuck Oh, all rights reserved
 * 이 프로그램은 한양대학교 ERICA 소프트웨어학부 재학생을 위한 교육용으로 제작되었습니다.
 */
#include <stdlib.h>
#include "pthread_pool.h"
#include <pthread.h>
#include <stdio.h>
#include <memory.h> // for memset

static task_t *dequeue(pthread_pool_t *pool);

/*
 * 풀에 있는 일꾼(일벌) 스레드가 수행할 함수이다.
 * FIFO 대기열에서 기다리고 있는 작업을 하나씩 꺼내서 실행한다.
 */
static void *worker(void *param)
{
    task_t *task;
    pthread_pool_t *pool = (pthread_pool_t*)param;
    while (1) {
        pthread_mutex_lock(&(pool->mutex));
        while (pool->q_size == 0) {
            pthread_cond_wait(&(pool->full), &(pool->mutex));
        }
        // doing some work
        task = dequeue(pool);
        if (task != NULL)
            task->function(task->param);
        pthread_mutex_unlock(&(pool->mutex));
    }
}

static void enqueue(pthread_pool_t *pool, task_t *t)
{
    pthread_mutex_lock(&(pool->mutex));
    while (pool->q_len == pool->q_size) {
        pthread_cond_wait(&(pool->empty), &(pool->mutex));
    }
    memmove(pool->q + pool->q_back * sizeof(task_t), t, sizeof(task_t));
    free(t);
    pool->q_len = pool->q_len + 1;
    pthread_mutex_unlock(&(pool->mutex));
    pool->q_back = (pool->q_back + 1) % pool->q_size;
    pthread_cond_broadcast(&(pool->full));
}

static task_t *dequeue(pthread_pool_t *pool)
{
    task_t result;
    task_t *task = (task_t*)malloc(sizeof(task_t) * pool->q_size);
    memmove(task, pool->q, sizeof(task_t) * pool->q_size);
    result = task[pool->q_front];
    pool->q_len = pool->q_len - 1;
    pool->q_front = (pool->q_front + 1) % pool->q_size;
    pthread_cond_broadcast(&(pool->empty));
    return &result;
}

/*
 * 스레드풀을 초기화한다. 성공하면 POOL_SUCCESS를, 실패하면 POOL_FAIL을 리턴한다.
 * bee_size는 일꾼(일벌) 스레드의 갯수이고, queue_size는 작업 대기열의 크기이다.
 * 대기열의 크기 queue_size가 최소한 일꾼의 수 bee_size보다 크거나 같게 만든다.
 */
int pthread_pool_init(pthread_pool_t *pool, size_t bee_size, size_t queue_size)
{
    if (bee_size > POOL_MAXBSIZE || queue_size > POOL_MAXQSIZE)
        return POOL_FAIL;

    task_t *queue;

    pthread_mutex_t mutex;
    pthread_cond_t full;
    pthread_cond_t empty;

    if (queue_size < bee_size) {
        queue = (task_t*)malloc(sizeof(task_t) * (bee_size));
        pool->q_size = bee_size;
    } else {
        queue = (task_t*)malloc(sizeof(task_t) * (queue_size));
        pool->q_size = queue_size;
    }

    pthread_t bee_[bee_size];

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&full, NULL);
    pthread_cond_init(&empty, NULL);

    pool->running = true;
    pool->q = queue;
    pool->q_front = 0;
    pool->q_back = 0;
    pool->q_len = 0;
    pool->bee = bee_;
    pool->bee_size = bee_size;
    pool->mutex = mutex;
    pool->full = full;
    pool->empty = empty;

    for (int i=0; i<bee_size; ++i) {
        if (pthread_create(&bee_[i], NULL, worker, (void *)pool) != 0) {
            fprintf(stderr, "pthread_create error\n");
            return POOL_FAIL;
        }
    }

    return POOL_SUCCESS;
}

/*
 * 스레드풀에서 실행시킬 함수와 인자의 주소를 넘겨주며 작업을 요청한다.
 * 스레드풀의 대기열이 꽉 찬 상황에서 flag이 POOL_NOWAIT이면 즉시 POOL_FULL을 리턴한다.
 * POOL_WAIT이면 대기열에 빈 자리가 나올 때까지 기다렸다가 넣고 나온다.
 * 작업 요청이 성공하면 POOL_SUCCESS를 리턴한다.
 */
int pthread_pool_submit(pthread_pool_t *pool, void (*f)(void *p), void *p, int flag)
{
    if (pool->q_len == pool->q_size) {
        if (flag == POOL_NOWAIT) {
            return POOL_FULL;
        } 
    }
    task_t *task = (task_t*)malloc(sizeof(task_t));
    task->function = f;
    task->param = p;
    enqueue(pool, task);

    return POOL_SUCCESS;
}

/*
 * 모든 일꾼 스레드를 종료하고 스레드풀에 할당된 자원을 모두 제거(반납)한다.
 * 락을 소유한 스레드를 중간에 철회하면 교착상태가 발생할 수 있으므로 주의한다.
 * 부모 스레드는 종료된 일꾼 스레드와 조인한 후에 할당된 메모리를 반납한다.
 * 종료가 완료되면 POOL_SUCCESS를 리턴한다.
 */
int pthread_pool_shutdown(pthread_pool_t *pool)
{
    pthread_t *bee = (pthread_t*)malloc(sizeof(pthread_t) * pool->bee_size);
    memmove(bee, pool->bee, sizeof(pthread_t) * pool->bee_size);

    for (int i=0; i<pool->bee_size; i++) {
        pthread_cancel(bee[i]);
    }

    for (int i=0; i<pool->bee_size; i++) {
        pthread_join(bee[i], NULL);
    }

    free(pool->q);
    pthread_mutex_destroy(&(pool->mutex));
    pthread_cond_destroy(&(pool->empty));
    pthread_cond_destroy(&(pool->full));

    return POOL_SUCCESS;
}
