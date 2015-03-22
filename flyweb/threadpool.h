#ifndef THREAD_POOL_H
#define THREAD_POOL_H 

#include <sys/types.h>
#include <pthread.h>
#include <assert.h>

typedef struct worker {
    void *(*routine) (void *arg);
    void *arg;
    struct worker *next;
} CThread_worker;

typedef struct 
{
   pthread_mutex_t queue_lock;
   pthread_cond_t queue_ready;

   CThread_worker *queue_head;
   int shutdown;
   pthread_t *threadid;

   int max_thread_num;
   int cur_queue_size;
} CThread_pool;

int pool_add_worker (void*(*routine) (void *arg), void *arg);
void *thread_routine (void *arg);
void pool_init(int max_thread_num);
int pool_destroy ();
static CThread_pool *pool = NULL;


#endif