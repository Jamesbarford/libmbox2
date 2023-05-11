#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bufctx.h"
#include "io.h"
#include "workers.h"

#define THREAD_COUNT (10)

void *
bufCtxReadLimits(void *buf)
{
    bufCtx *self = (bufCtx *)buf;
    ssize_t want = 0;
    ssize_t rbytes = 0;

    memset(self->buf, '\0', 20);

    printf("%d\n", self->id);

    want = self->end_offset - self->start_offset;
    rbytes = __pread(self->fd, self->buf, want, self->start_offset);

    if (rbytes == -1) {
        fprintf(stderr, "Failed to __pread\n");
        exit(1);
    }

    self->buf[rbytes] = '\0';
    self->buflen = rbytes;
    return NULL;
}

void *
bufCtxSetOffsetCallback(void *buf)
{
    bufCtx *ctx = (bufCtx *)buf;
    bufCtxCollectFullBuffer(ctx);

    /* This shows we have a race condition */
    if (ctx->debug_has_freed) {
        printf("............................................................>>>>\n");
        exit(1);
    }
    return NULL;
}

void
noThreads(int fd, size_t file_size)
{
    for (int i = 0; i < EXAMPLE_LEN; ++i) {
        printf("idx :: %d\n", i);
        bufCtx *ctx = bufCtxNew(i, fd, file_size);
        if (ctx == NULL) {
            printf("messed up\n");
            break;
        }
        ctx->start_offset = ctx->buf_offset = ctx->file_offset = i;
        bufCtxCollectFullBuffer(ctx);
        printf("------------\n");
        bufCtxRelease(ctx);
    }
}

void
withThreads(int fd, size_t file_size)
{
    workerPool *pool = NULL;
    size_t chunk_size = 0;
    int non_nulls = 0;
    bufCtx *contexts[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; ++i) {
        contexts[i] = bufCtxNew(i, fd, file_size);
    }

    chunk_size = file_size / THREAD_COUNT;

    pool = workerPoolNew(THREAD_COUNT);

    for (int i = 0; i < THREAD_COUNT; ++i) {
        bufCtx *ctx = contexts[i];
        bufCtxInit(ctx, i, fd, file_size);
        ctx->file_offset = ctx->start_offset = (i * chunk_size);

        printf("[%d]offset: %zu\n", ctx->id, ctx->start_offset);
        workerPoolEnqueue(pool, bufCtxSetOffsetCallback, ctx);
    }

    workerPoolWait(pool);

    /* Set all of the offsets */
    for (int i = 1; i < THREAD_COUNT; ++i) {
        bufCtx *ctx_prev = contexts[i - 1];
        bufCtx *ctx = contexts[i];

        ctx_prev->end_offset = ctx->start_offset;
    }

    /* Set the last ones offset to the length of the file */
    bufCtx *ctx = contexts[THREAD_COUNT - 1];
    ctx->end_offset = file_size;

    /* Filter out contexts with a 0 read size, and queue valid contexts */
    for (int i = 0; i < THREAD_COUNT; ++i) {
        bufCtx *ctx = contexts[i];

        if (ctx->end_offset - ctx->start_offset <= 0) {
            bufCtxRelease(ctx);
            //    contexts[i] = NULL;
        } else {
            workerPoolEnqueue(pool, bufCtxReadLimits, ctx);
        }
    }

    workerPoolWait(pool);

    printf("Now with full buffers\n");
    for (int i = 0; i < THREAD_COUNT; ++i) {
        bufCtx *ctx = contexts[i];
        if (ctx->debug_has_freed == 0) {
            printf("[%d]%s\n", ctx->id, ctx->buf);
            bufCtxRelease(ctx);
        }
    }
    //workerPoolWait(pool);
    workerPoolRelease(pool);
}

int
main(void)
{
    ioInit();
    srand(time(NULL));
    struct _stat sb;
    size_t file_size = 0;

    int fd = __open("example.txt", _O_RDONLY, 0666);
    __fstat(fd, &sb);

    file_size = sb.st_size;
    withThreads(fd, file_size);

    ioClear();
}
