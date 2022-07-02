#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "threadpool.h"


typedef struct node {
  void (*fn)(tpool_t, void*);
  void *arg;
  struct node *next;
} node;

struct tpool {

  /* TO BE COMPLETED BY THE STUDENT */
  int num_threads; //number of threads we want
  node *head; //queue head
  node *tail; //queue tail
  int queue_size;
  pthread_mutex_t lock; //lock for queue
  // pthread_cond_t empty_queue;
  // pthread_cond_t non_empty_queue;
  pthread_t *threads; //thread array
};

/* Function executed by each pool worker thread. This function is
 * responsible for running individual tasks. The function continues
 * running as long as either the pool is not yet joined, or there are
 * unstarted tasks to run. If there are no tasks to run, and the pool
 * has not yet been joined, the worker thread must be blocked.
 * 
 * Parameter: param: The pool associated to the thread.
 * Returns: nothing.
 */
void *run_tasks(void *param) {
  tpool_t pool = (tpool_t) param;
  node *current;
  while(1) {
    current = pool->head;
    pool->queue_size--;
    pthread_mutex_lock(&pool->lock);
    // while(pool->queue_size == 0) {
    //   pthread_mutex_unlock(&pool->lock);
    //   pthread_cond_wait(&pool->non_empty_queue, &pool->lock);
    // }
    if (pool->queue_size == 0) {
      pool->head = NULL;
      pool->tail = NULL;
    } else {
      pool->head = current->next;
    }
    // if (!pool->queue_size) {
    //   pthread_cond_signal(&pool->empty_queue);
    // }
    pthread_mutex_unlock(&pool->lock);
    (current->fn)(pool, current->arg);
    free(current);
  }
}


void tpool_init(tpool_t pool, unsigned int num_threads) {
  pool->num_threads = num_threads;
  pool->threads = (pthread_t* )malloc(num_threads * sizeof(pthread_t));
  for (int i = 0; i < num_threads; i++) {
    pool->threads[i] = (pthread_t) malloc(sizeof(pthread_t));
  }
  pool->head = NULL;
  pool->tail = NULL;
  pthread_mutex_init(&pool->lock, NULL);
  // pthread_cond_init(&pool->empty_queue, NULL);
  // pthread_cond_init(&pool->non_empty_queue, NULL);
}

/* Creates (allocates) and initializes a new thread pool. Also creates
 * `num_threads` worker threads associated to the pool, so that
 * `num_threads` tasks can run in parallel at any given time.
 *
 * Parameter: num_threads: Number of worker threads to be created.
 * Returns: a pointer to the new thread pool object.
 */
tpool_t tpool_create(unsigned int num_threads) {

  /* TO BE COMPLETED BY THE STUDENT */
  tpool_t pool;
  pool = (tpool_t) malloc(sizeof(tpool_t));
  if (!pool) {
    perror("Unable to allocate memory for the threadpool\n");
    return NULL;
  }
  
  tpool_init(pool, num_threads);

  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&pool->threads[i], NULL, &run_tasks, pool))
    {
      printf("unable to create thread\n");
    }
  
  }

  return (tpool_t) pool;
}

/* Schedules a new task, to be executed by one of the worker threads
 * associated to the pool. The task is represented by function `fun`,
 * which receives the pool and a generic pointer as parameters. If any
 * of the worker threads is available, `fun` is started immediately by
 * one of the worker threads. If all of the worker threads are busy,
 * `fun` is scheduled to be executed when a worker thread becomes
 * available. Tasks are retrieved by individual worker threads in the
 * order in which they are scheduled, though due to the nature of
 * concurrency they may not start exactly in the same order. This
 * function returns immediately, and does not wait for `fun` to
 * complete.
 *
 * Parameters: pool: the pool that is expected to run the task.
 *             fun: the function that should be executed.
 *             arg: the argument to be passed to fun.
 */
void tpool_schedule_task(tpool_t pool, void (*fun)(tpool_t, void *), void *arg) {

  /* TO BE COMPLETED BY THE STUDENT */
  node *current;
  current = (node*) malloc(sizeof(node));
  if (!current) {
    printf("Unable to allocate memory for queue");
    return;
  }

  current->fn = fun;
  current->arg = arg;
  current->next = NULL;

  pthread_mutex_lock(&pool->lock);
  if(pool->queue_size == 0) {
    pool->head = current;
    pool->tail = current;
    // pthread_cond_signal(&pool->non_empty_queue);
  } else {
    pool->tail->next = current;
    pool->tail = current;
  }
  pool->queue_size++;
  pthread_mutex_unlock(&pool->lock);
}

/* Blocks until the thread pool has no more scheduled tasks; then,
 * joins all worker threads, and frees the pool and all related
 * resources. Once this function returns, no additional tasks can be
 * scheduled, and the thread pool resources are destroyed/freed.
 *
 * Parameters: pool: the pool to be joined.
 */
void tpool_join(tpool_t pool) {

  /* TO BE COMPLETED BY THE STUDENT */
  for(int i = 0; i < pool->num_threads; i++) {
    pthread_join(pool->threads[i], NULL);
  }
  pthread_mutex_destroy(&pool->lock);
  free(pool->threads);
  free(pool);
}
