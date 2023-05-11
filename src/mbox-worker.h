/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __WORKERS_H
#define __WORKERS_H

#include <pthread.h>
#include <stddef.h>

#include "mbox-list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* First argument is the priv_data from workerpool second is whatever was called
 * with Enqueue */
typedef void mboxWorkerCallback(void *priv_data, void *argv);

typedef struct mboxWorkerJob mboxWorkerJob;
typedef struct mboxWorker {
    pthread_t th;
    int id;
} mboxWorker;

typedef struct mboxSemaphore {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    volatile int val;
} mboxSemaphore;

typedef struct mboxWorkerPool {
    mboxList *jobs;                 /* Work to be done */
    volatile size_t worker_count;   /* How many threads we have in the pool */
    volatile size_t alive_threads;  /* Threads that are alive */
    volatile size_t active_threads; /* Work queued, only decemented when the
                                     * work is complete
                                     */
    int run;                        /* Keep running the thread pool */
    pthread_cond_t has_work;
    pthread_cond_t no_work;
    pthread_mutex_t lock;
    pthread_mutex_t qlock;
    mboxSemaphore *sem;
    mboxWorker *workers; /* Array of workers */
    void *priv_data; /* Passed as the first argument to onComplete, can be any
                        value. Simulating a closure */
} mboxWorkerPool;

#define mboxWorkerPoolSetPrivData(p, d) (p->priv_data = d)

void mboxWorkerPoolEnqueue(mboxWorkerPool *pool, mboxWorkerCallback *callback,
        void *work);
void mboxWorkerPoolWait(mboxWorkerPool *pool);
mboxWorkerPool *mboxWorkerPoolNew(size_t worker_count);
void mboxWorkerPoolRelease(mboxWorkerPool *pool);

#ifdef __cplusplus
}
#endif

#endif
