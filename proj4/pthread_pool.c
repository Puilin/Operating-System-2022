/*
 * Copyright 2022. Heekuck Oh, all rights reserved
 * 이 프로그램은 한양대학교 ERICA 소프트웨어학부 재학생을 괴롭히기 위한 고문용으로 제작되었습니다.
 */
#include <stdlib.h>
#include "pthread_pool.h"

/*
 * 풀에 있는 일꾼(일벌) 스레드가 수행할 함수이다.
 * FIFO 대기열에서 기다리고 있는 작업을 하나씩 꺼내서 실행한다.
 * 작업이 있으면 실행, 아니면 대기
 */
static void *worker(void *param)
{
    pthread_pool_t *pool = (pthread_pool_t *)param;
    while(pool->running == true){
        pthread_mutex_lock(&pool->mutex);
        while(pool->q_len == 0){
            if(pool->running == false){
                pthread_mutex_unlock(&pool->mutex);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&pool->full, &pool->mutex);
        }
        /* CS start */
        pool->q_front = (pool->q_front + 1) % pool->q_size;
        pool->q_len -= 1;
        pool->q->function(pool->q->param);
        /* CS end */
        pthread_mutex_unlock(&pool->mutex);
        pthread_cond_signal(&pool->empty);
    }
    pthread_exit(NULL);
}

/*
 * 스레드풀을 초기화한다. 성공하면 POOL_SUCCESS를, 실패하면 POOL_FAIL을 리턴한다.
 * bee_size는 일꾼(일벌) 스레드의 갯수이고, queue_size는 작업 대기열의 크기이다.
 * 대기열의 크기 queue_size가 최소한 일꾼의 수 bee_size보다 크거나 같게 만든다.
 */
int pthread_pool_init(pthread_pool_t *pool, size_t bee_size, size_t queue_size)
{
    /*
     * bee_size와 queue_size가 적절한 값인지 먼저 검사
     */
    if(bee_size > POOL_MAXBSIZE || queue_size > POOL_MAXQSIZE) return POOL_FAIL;
    /*
     * queue_size 최적화
     */
    if(queue_size < bee_size) queue_size = bee_size;
    /*
     * 공간 할당 파트
     */
    pool->q = (task_t *)malloc(sizeof(task_t) * queue_size);
    pool->bee = (pthread_t *)malloc(sizeof(pthread_t) * bee_size);
    /*
     * 초기화 파트
     */
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->full, NULL);
    pthread_cond_init(&pool->empty, NULL);
    pool->q_size = queue_size;
    pool->bee_size = bee_size;
    pool->q_len = 0;
    pool->q_front = 0;
    pool->running = true;
    /*
     * 스레드 생성 파트
     */
    for(int i = 0; i < bee_size; i++){
        pthread_create(pool->bee+i, NULL, worker, (void *)pool);
    }
    /*
     * return 파트
     */
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
    pthread_mutex_lock(&pool->mutex);
    pool->q_len +=1;
    while(pool->q_len > pool->q_size){
        if (flag == POOL_NOWAIT) {
            pthread_mutex_unlock(&pool->mutex);
            return POOL_FULL;
        }
        else pthread_cond_wait(&pool->empty, &pool->mutex);
    }
    /* CS start */
    pool->q->function = f;
    pool->q->param = p;
    /* CS end */
    pthread_mutex_unlock(&pool->mutex);
    pthread_cond_signal(&pool->full);

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
    pthread_mutex_lock(&pool->mutex);
    pool->running = false;
    pthread_mutex_unlock(&pool->mutex);
    /*
     * 조건 변수에서 대기하고 있을지도 모를 스레드 깨우기
     */
    pthread_cond_broadcast(&pool->full);
    pthread_cond_broadcast(&pool->empty);
    /*
     * 스레드가 끝날 때까지 기다림
     */
    for(int i = 0; i < pool->bee_size; i++){
        pthread_join(*(pool->bee+i), NULL);
    }
    /*
     * 모든 자원 반납
     */
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->full);
    pthread_cond_destroy(&pool->empty);
    free(pool->q);
    free(pool->bee);
    return POOL_SUCCESS;
}
