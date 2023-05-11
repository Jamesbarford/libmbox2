#ifndef __WORKERS_H
#define __WORKERS_H

#include <pthread.h>

#include "list.h"

typedef void *workerCallback(void *argv);
/* First argument is the priv_data from workerpool second is the return value
 * from the workerCallBack */
typedef void workerOnComplete(void *priv_data, void *argv);

typedef struct worker {
    pthread_t th;
    int id;
} worker;

typedef struct workerPool {
    list *jobs;          /* Work to be done */
    size_t worker_count; /* How many threads we have in the pool */
    size_t job_count; /* Work queued, only decemented when the work is complete
                       */
    int run;          /* Keep running the thread pool */
    pthread_cond_t has_work;
    pthread_cond_t no_work;
    pthread_mutex_t lock;
    worker *workers;              /* Array of workers */
    workerOnComplete *onComplete; /* Called when the job is done */
    void *priv_data; /* Passed as the first argument to onComplete, can be any
                        value. Simulating a closure */
} workerPool;

#define workerPoolSetOnComplete(p, cb) (p->onComplete = cb)
#define workerPoolSetPrivData(p, d) (p->priv_data = d)

void workerPoolEnqueue(workerPool *pool, workerCallback *callback, void *work);
void workerPoolWait(workerPool *pool);
workerPool *workerPoolNew(size_t worker_count);
void workerPoolRelease(workerPool *pool);

#endif
