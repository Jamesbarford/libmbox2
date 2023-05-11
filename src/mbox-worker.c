/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <sys/types.h>

#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include "mbox-list.h"
#include "mbox-logger.h"
#include "mbox-worker.h"

typedef struct mboxWorkerJob {
    mboxWorkerCallback *callback;
    void *argv;
    struct mboxWorkerJob *next;
} mboxWorkerJob;

static mboxSemaphore *
mboxSemNew(void)
{
    mboxSemaphore *sem = malloc(sizeof(mboxSemaphore));
    sem->val = 0;
    pthread_cond_init(&sem->cond, NULL);
    pthread_mutex_init(&sem->lock, NULL);
    return sem;
}

static void
mboxSemaphoreSignal(mboxSemaphore *sem)
{
    pthread_mutex_lock(&sem->lock);
    sem->val = 1;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->lock);
}

static void
mboxSemaphoreWait(mboxSemaphore *sem)
{
    pthread_mutex_lock(&sem->lock);
    while (sem->val != 1) {
        pthread_cond_wait(&sem->cond, &sem->lock);
    }
    sem->val = 0;
    pthread_mutex_unlock(&sem->lock);
}

static mboxWorkerJob *
mboxWorkerJobNew(mboxWorkerCallback *callback, void *argv)
{
    mboxWorkerJob *job = (mboxWorkerJob *)malloc(sizeof(mboxWorkerJob));
    job->callback = callback;
    job->argv = argv;
    job->next = NULL;
    return job;
}

/* Add a job to the queue */
void
mboxWorkerPoolEnqueue(mboxWorkerPool *pool, mboxWorkerCallback *callback,
        void *argv)
{
    mboxWorkerJob *job = mboxWorkerJobNew(callback, argv);
    pthread_mutex_lock(&pool->qlock);
    mboxListAddTail(pool->jobs, job);
    mboxSemaphoreSignal(pool->sem);
    pthread_mutex_unlock(&pool->qlock);
}

static mboxWorkerJob *
mboxWorkerPoolDequeue(mboxWorkerPool *pool)
{
    pthread_mutex_lock(&pool->qlock);
    mboxWorkerJob *job = mboxListRemoveHead(pool->jobs);

    if (job) {
        mboxSemaphoreSignal(pool->sem);
    }

    pthread_mutex_unlock(&pool->qlock);
    return job;
}

/* Wait for all jobs in the pool to complete */
void
mboxWorkerPoolWait(mboxWorkerPool *pool)
{
    pthread_mutex_lock(&pool->lock);
    while (pool->jobs->len || pool->active_threads != 0) {
        pthread_cond_wait(&pool->no_work, &pool->lock);
    }
    pthread_mutex_unlock(&pool->lock);
}

static void *
mboxWorkerPoolMain(void *argv)
{
    mboxWorkerPool *pool = (mboxWorkerPool *)argv;
    mboxWorkerJob *job = NULL;

    pthread_mutex_lock(&pool->lock);
    pool->alive_threads++;
    pthread_mutex_unlock(&pool->lock);

    while (1) {

        mboxSemaphoreWait(pool->sem);

        pthread_mutex_lock(&pool->lock);
        pool->active_threads++;
        pthread_mutex_unlock(&pool->lock);

        job = mboxWorkerPoolDequeue(pool);
        if (job) {
            job->callback(pool->priv_data, job->argv);
            free(job);
        }

        pthread_mutex_lock(&pool->lock);
        pool->active_threads--;

        if (pool->active_threads == 0) {
            pthread_cond_signal(&pool->no_work);
        }

        pthread_mutex_unlock(&pool->lock);
    }

    pool->worker_count--;
    if (pool->active_threads == 0) {
        pthread_cond_signal(&pool->no_work);
    }
    pthread_mutex_unlock(&pool->lock);
    pthread_exit(NULL);
    return NULL;
}

static void
workerSpawn(mboxWorkerPool *pool, size_t worker_count)
{
    mboxWorker *workers = (mboxWorker *)malloc(
            sizeof(mboxWorker) * worker_count);

    pool->workers = workers;
    pool->worker_count = worker_count;

    for (size_t i = 0; i < worker_count; ++i) {
        mboxWorker *worker = &workers[i];
        worker->id = i;
        pthread_create(&worker->th, NULL, mboxWorkerPoolMain, pool);
        pthread_detach(worker->th);
    }
}

mboxWorkerPool *
mboxWorkerPoolNew(size_t worker_count)
{
    mboxWorkerPool *pool = (mboxWorkerPool *)malloc(sizeof(mboxWorkerPool));
    pool->jobs = mboxListNew();
    pool->run = 1;
    pool->worker_count = worker_count;
    pool->active_threads = 0;
    pool->priv_data = NULL;
    pool->sem = mboxSemNew();
    pool->alive_threads = 0;

    pthread_cond_init(&pool->no_work, NULL);
    pthread_cond_init(&pool->has_work, NULL);
    pthread_mutex_init(&pool->lock, NULL);
    pthread_mutex_init(&pool->qlock, NULL);
    workerSpawn(pool, pool->worker_count);
    while (pool->alive_threads != pool->worker_count)
        ;

    return pool;
}

/* Wait for all jobs to complete and then destroy all of the workers in the
 * pool, only call if we're trying to free the pool */
static void
mboxWorkerPoolStop(mboxWorkerPool *pool)
{
    pthread_mutex_lock(&pool->lock);
    while (pool->active_threads != 0) {
        pthread_cond_wait(&pool->has_work, &pool->lock);
    }
    pool->run = 0;
    pthread_cond_broadcast(&pool->has_work);
    pthread_mutex_unlock(&pool->lock);

    for (size_t i = 0; i < pool->worker_count; ++i) {
        pthread_join(pool->workers[i].th, NULL);
    }

    pool->worker_count = 0;
    pthread_cond_signal(&pool->no_work);
}

void
mboxWorkerPoolRelease(mboxWorkerPool *pool)
{
    if (pool) {
        mboxWorkerPoolStop(pool);

        pthread_cond_destroy(&pool->has_work);
        pthread_cond_destroy(&pool->no_work);
        pthread_mutex_destroy(&pool->lock);

        mboxListRelease(pool->jobs);
        free(pool->workers);
        free(pool);
    }
}
