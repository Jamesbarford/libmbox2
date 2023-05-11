#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "workers.h"

typedef struct workerJob {
    void *(*complete)(void *argv);
    void *argv;
} workerJob;

static workerJob *
workerJobNew(workerCallback *callback, void *work)
{
    workerJob *j = malloc(sizeof(workerJob));
    j->argv = work;
    j->complete = callback;
    return j;
}

void
workerPoolEnqueue(workerPool *pool, workerCallback *callback, void *work)
{
    workerJob *job = workerJobNew(callback, work);
    pthread_mutex_lock(&pool->lock);

    pool->job_count++;
    listAddTail(pool->jobs, job);

    pthread_cond_broadcast(&pool->has_work);
    pthread_mutex_unlock(&pool->lock);
}

/* Wait for all jobs in the pool to complete */
void
workerPoolWait(workerPool *pool)
{
    pthread_mutex_lock(&pool->lock);
    while (pool->job_count != 0) {
        pthread_cond_wait(&pool->no_work, &pool->lock);
    }
    pthread_mutex_unlock(&pool->lock);
}

/* The main worker loop */
static void *
workerMain(void *argv)
{
    workerPool *pool = (workerPool *)argv;
    list *jobs = pool->jobs;
    workerJob *job = NULL;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        if (pool->run == 0) {
            break;
        }

        while (pool->job_count == 0 && pool->run) {
            pthread_cond_wait(&pool->has_work, &pool->lock);
        }

        job = listRemoveHead(jobs);
        pthread_mutex_unlock(&pool->lock);

        if (job) {
            void *done = job->complete(job->argv);
            if (pool->onComplete) {
                pool->onComplete(pool->priv_data, done);
            }
            free(job);
        } else {
            /* No need to aquire a lock or decrement active count if the thread
             * has done no work as we are using an unsigned value it will over
             * flow making our wait condition spin forever */
            continue;
        }

        pthread_mutex_lock(&pool->lock);
        /* Decrement only when the job is done */
        pool->job_count--;

        if (pool->run && pool->job_count == 0) {
            pthread_cond_signal(&pool->no_work);
        }

        pthread_mutex_unlock(&pool->lock);
    }

    pool->worker_count--;
    if (pool->job_count == 0) {
        pthread_cond_signal(&pool->no_work);
    }
    pthread_mutex_unlock(&pool->lock);
    pthread_exit(NULL);

    return NULL;
}

static void
workerSpawn(workerPool *pool, size_t worker_count)
{
    worker *workers = (worker *)malloc(sizeof(worker) * worker_count);

    for (size_t i = 0; i < worker_count; ++i) {
        worker *w = &workers[i];
        w->id = i;
        pthread_create(&w->th, NULL, workerMain, pool);
    }

    pool->worker_count = worker_count;
    pool->workers = workers;
}

workerPool *
workerPoolNew(size_t worker_count)
{
    workerPool *pool = (workerPool *)malloc(sizeof(workerPool));
    pool->jobs = listNew();
    pool->run = 1;
    pool->worker_count = worker_count;
    pool->job_count = 0;
    pool->onComplete = NULL;

    pthread_cond_init(&pool->no_work, NULL);
    pthread_cond_init(&pool->has_work, NULL);
    pthread_mutex_init(&pool->lock, NULL);
    workerSpawn(pool, pool->worker_count);

    return pool;
}

/* Wait for all jobs to complete and then destroy all of the workers in the
 * pool, only call if we're trying to free the pool */
static void
workerPoolStop(workerPool *pool)
{
    pthread_mutex_lock(&pool->lock);
    while (pool->job_count != 0) {
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
workerPoolRelease(workerPool *pool)
{
    if (pool) {
        workerPoolStop(pool);

        pthread_cond_destroy(&pool->has_work);
        pthread_cond_destroy(&pool->no_work);
        pthread_mutex_destroy(&pool->lock);

        listRelease(pool->jobs);
        free(pool->workers);
        free(pool);
    }
}
