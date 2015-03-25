#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <assert.h>
#include "threadpool.h"
void pool_init(int max_thread_num) {
    pool = (CThread_pool *) malloc (sizeof (CThread_pool));
    pthread_mutex_init (&(pool->queue_lock),NULL);
    pthread_cond_init (&(pool->queue_ready), NULL);
    pool->queue_head = NULL;
    pool->max_thread_num = max_thread_num;
    pool->cur_queue_size = 0;
    pool->shutdown = 0;
    pool->threadid =
        (pthread_t *) malloc (max_thread_num * sizeof(pthread_t));
    int i = 0;
    for(int i = 0; i < max_thread_num; i++) 
       pthread_create (&(pool->threadid[i]),NULL,thread_routine,NULL);   
}

int pool_add_worker(void *(*routine) (void *arg), void *arg){
    CThread_worker *newworker = (CThread_worker *) malloc (sizeof (CThread_worker));
    newworker->routine = routine;
    newworker->arg = arg;
    newworker->next = NULL;
    pthread_mutex_lock (&(pool->queue_lock));
    /*将任务加入到等待队列中*/
    CThread_worker *member = pool->queue_head;
    if(member != NULL) {
        while(member->next != NULL)
            member = member->next;
        member->next = newworker;
    }
    else 
        pool->queue_head = newworker;
    assert(pool->queue_head != NULL);
    pool->cur_queue_size++;
    /*等待队列中有任务了，唤醒一个等待线程.*/
    pthread_mutex_unlock(&(pool->queue_lock));
    pthread_cond_signal(&(pool->queue_ready));
    return 0;
}
int pool_destroy (){
    if(pool->shutdown)
        return -1;
    pool->shutdown = 1;
    /*唤醒所有等待线程*/
    pthread_cond_broadcast(&(pool->queue_ready));
    int i;
    for(i = 0; i < pool->max_thread_num; i++)
        pthread_join(pool->threadid[i],NULL);
    free (pool->threadid);
    /*销毁等待队列*/
    CThread_worker *head = NULL;
    while(pool->queue_head != NULL) {
        head = pool->queue_head;
        pool->queue_head = pool->queue_head->next;
        free(head);
    }
    pthread_mutex_destroy(&(pool->queue_lock));
    pthread_cond_destroy(&(pool->queue_ready));
      free(pool);
       pool=NULL;
        return 0;
  }
    void * thread_routine (void *arg){
        printf("startting thread 0x%x\n", pthread_self());
        while(1){
            pthread_mutex_lock(&(pool->queue_lock));
            /*如果等待队列为0并且不销毁线程池，则处于阻塞状态*/
            while(pool->cur_queue_size == 0 && !pool->shutdown){
                printf("thread 0x%x is waitting\n", pthread_self());
                pthread_cond_wait(&(pool->queue_ready),&(pool->queue_lock));
            }
            /*销毁线程池*/
            if(pool->shutdown){
                pthread_mutex_unlock(&(pool->queue_lock));
                printf("thread 0x%x will exit\n", pthread_self());
                pthread_exit(NULL);
            }
            printf("thread 0x%x is starting to work\n", pthread_self());
            assert(pool->cur_queue_size != 0);
            assert(pool->queue_head != NULL);
            /*等待队列长度减去1，并取出链表中的头元素*/
            pool->cur_queue_size--;
            CThread_worker *worker = pool->queue_head;
            pool->queue_head = worker->next;
            pthread_mutex_unlock(&(pool->queue_lock));
            /*调用回调函数，执行任务*/
            (*(worker->routine)) (worker->arg);
            free(worker);
            worker = NULL;
        }
        pthread_exit(NULL);
    }
  