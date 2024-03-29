#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "threadpool.h"


/* Begin prototypes*/
void *run_tasks(void *);
int tpool_init(tpool_t, unsigned int);
/*End prototypes*/

typedef struct node {
  void (*fn)(tpool_t, void*);
  void *arg;
  struct node *next;
} node_t;

struct tpool {
  int num_threads; //number of threads we want
  node_t *head; //queue head
  node_t *tail; //queue tail
  int queue_size; //current size of queue
  pthread_mutex_t lock; //lock for queue
  pthread_cond_t empty_queue;
  pthread_cond_t non_empty_queue;
  pthread_cond_t tasks_running; //condition variable used to make sure all tasks are complete before we can shutdown
  pthread_t *threads; //thread array
  int is_killable; //check if the pool is ready to be killed
  int num_tasks_running; //total currently running tasks
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
  node_t *current;
  while(1) {
    pthread_mutex_lock(&pool->lock);
    while(pool->queue_size == 0) {
      if(pool->is_killable && pool->num_tasks_running == 0) {
        pthread_mutex_unlock(&pool->lock);
        pthread_exit(NULL);
      }
      pthread_cond_wait(&pool->non_empty_queue, &pool->lock);
      if(pool->is_killable && pool->num_tasks_running == 0) {
        pthread_mutex_unlock(&pool->lock);
        pthread_exit(NULL);
      }
    }
    current = pool->head;
    pool->queue_size--;
    if (pool->queue_size == 0) {
      pool->head = NULL;
      pool->tail = NULL;
    } else {
      pool->head = current->next;
    }
    if (pool->queue_size == 0 && !pool->is_killable) {
      pthread_cond_signal(&pool->empty_queue);
    }
    pthread_mutex_unlock(&pool->lock);
    (current->fn)(pool, current->arg);

    pthread_mutex_lock(&pool->lock);
    pool->num_tasks_running--;
    if(!pool->num_tasks_running) {
      pthread_cond_broadcast(&pool->tasks_running); //no tasks are running, and we can kill the threadpool
    }
    pthread_mutex_unlock(&pool->lock);

    free(current);
  }
}


int tpool_init(tpool_t pool, unsigned int num_threads) {
  pool->num_threads = num_threads;
  pool->threads = (pthread_t* )malloc(num_threads * sizeof(pthread_t));
  pool->head = NULL;
  pool->tail = NULL;
  pool->queue_size = 0;
  pool->is_killable = 0;
  pool->num_tasks_running = 0;
  if (pthread_mutex_init(&pool->lock, NULL)) {
    printf("Error initializing pool lock");
    return 0;
  }
  if (pthread_cond_init(&pool->empty_queue, NULL)) {
    printf("Error initializing condition variable empty_queue");
    return 0;
  }
  if (pthread_cond_init(&pool->non_empty_queue, NULL)) {
    printf("Error initializing condition variable non_empty_queue");
    return 0;
  }
  if (pthread_cond_init(&pool->tasks_running, NULL)) {
    printf("Error initializing condition variable tasks running");
    return 0;
  }
  return 1;
}

/* Creates (allocates) and initializes a new thread pool. Also creates
 * `num_threads` worker threads associated to the pool, so that
 * `num_threads` tasks can run in parallel at any given time.
 *
 * Parameter: num_threads: Number of worker threads to be created.
 * Returns: a pointer to the new thread pool object.
 */
tpool_t tpool_create(unsigned int num_threads) {
  tpool_t pool;
  pool = (tpool_t) malloc(sizeof(struct tpool));
  if (!pool) {
    printf("Unable to allocate memory for the threadpool\n");
    return NULL;
  }
  
  if (!tpool_init(pool, num_threads)) {
    return NULL;
  }

  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&pool->threads[i], NULL, &run_tasks, pool))
    {
      printf("Unable to create thread:%d\n", i);
      return NULL;
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

  node_t *current;
  current = (node_t*) malloc(sizeof(node_t));
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
    pthread_cond_signal(&pool->non_empty_queue); //signal threads that queue is now populated
  } else {
    pool->tail->next = current;
    pool->tail = current;
  }
  pool->queue_size++;
  pool->num_tasks_running++;
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

  pthread_mutex_lock(&pool->lock);
  while(pool->queue_size != 0) {
    pthread_cond_wait(&pool->empty_queue, &pool->lock);
  }
  pool->is_killable = 1;
  while(pool->num_tasks_running) {
    pthread_cond_wait(&pool->tasks_running, &pool->lock);
  }
  pthread_cond_broadcast(&pool->non_empty_queue);
  pthread_mutex_unlock(&pool->lock);
  for(int i = 0; i < pool->num_threads; i++) {
    pthread_join(pool->threads[i], NULL);
  }
  free(pool->threads);
  pthread_mutex_destroy(&pool->lock);
  pthread_cond_destroy(&pool->empty_queue);
  pthread_cond_destroy(&pool->non_empty_queue);
  pthread_cond_destroy(&pool->tasks_running);
  free(pool);
}
