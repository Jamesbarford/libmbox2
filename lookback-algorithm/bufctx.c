#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bufctx.h"
#include "io.h"

/* Read into the ctx buffer from a buffer offset and a file offset, this is a
 * mock of 'pread' where 'EXAMPLE' is the Fake file we are reading from in
 * chunks */
ssize_t
bufCtxRead(bufCtx *ctx, int buf_offset, int file_offset)
{
    ssize_t rbytes = 0;

    ctx->err = 0;
    rbytes = __pread(ctx->fd, ctx->buf + buf_offset, BUF_READ_SIZE,
            file_offset);

    if (rbytes > 0) {
        ctx->buflen += rbytes;
        ctx->buf[ctx->buflen] = '\0';
    } else if (rbytes == 0) {
        ctx->err = EOF;
    }

    return rbytes;
}

/* Go backwards until pattern 'AA' */
void
bufCtxSeekStart(bufCtx *ctx)
{
    struct _stat sb;
    ssize_t offset = ctx->file_offset;
    ssize_t jumpsize = 0;
    ssize_t rbytes = 0;
    int found_offset = 0;
    int idx = 0;

    __fstat(ctx->fd, &sb);

    /* 4% of the file size */
    jumpsize = sb.st_size * (4.0 / 100.0);

    while (1) {
        offset = offset - jumpsize < 0 ? 0 : offset - jumpsize;

        rbytes = bufCtxRead(ctx, 0, offset);
        ctx->buflen = rbytes;

        if (rbytes == 0 || ctx->err == 1) {
            return;
        }

        for (idx = ctx->buflen - 1; idx >= 0; --idx) {
            if (ctx->buf[idx] == 'A' && ctx->buf[idx + 1] == 'A') {
                ctx->start_offset = offset + idx;
                found_offset = 1;
                break;
            }
        }

        if (found_offset) {
            break;
        }
    }
    ctx->buflen = 0;
    ctx->buf_offset = 0;
    /* Current file offset is now the start offset */
    ctx->file_offset = ctx->start_offset;
}

/* This should go from one 'AA' till the next 'AA' */
void
bufCtxCollectFullBuffer(bufCtx *ctx)
{
    bufCtxSeekStart(ctx);

    ssize_t rbytes = bufCtxRead(ctx, ctx->buf_offset, ctx->file_offset);
    printf("[%d][%zu]%s\n", ctx->id, ctx->start_offset, ctx->buf);
    // ctx->file_offset += rbytes;
    ctx->file_offset += rbytes;
}

void
bufCtxInit(bufCtx *ctx, int id, int fd, size_t file_size)
{
    ctx->file_size = file_size;
    ctx->id = id;
    ctx->fd = fd;
    ctx->buf = malloc(sizeof(char) * ctx->file_size);
    ctx->buflen = 0;
    ctx->buf_offset = 0;
    ctx->file_offset = -1;
    ctx->start_offset = -1;
    ctx->debug_has_freed = 0;
    ctx->end_offset = -1;
    ctx->err = 0;
}

bufCtx *
bufCtxNew(int id, int fd, size_t file_size)
{
    bufCtx *ctx = malloc(sizeof(bufCtx));
    bufCtxInit(ctx, id, fd, file_size);
    return ctx;
}

void
bufCtxRelease(bufCtx *ctx)
{
    if (ctx) {
        ctx->debug_has_freed = 1;
        if (ctx->buf != NULL) {
            free(ctx->buf);
        }
        // free(ctx);
    }
}
