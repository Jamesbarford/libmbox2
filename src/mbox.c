/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mbox-buf.h"
#include "mbox-io.h"
#include "mbox-list.h"
#include "mbox-logger.h"
#include "mbox-msg.h"
#include "mbox-parser.h"
#include "mbox-worker.h"
#include "mbox.h"

typedef struct mbox {
    int readfd;
    int writefd;
    int read_refcount;
    int write_refcount;
    int err;
    int ready;
    size_t file_size;
    size_t thread_count;
    size_t context_len;
    mboxParserCtx *contexts;
    mboxList *completed_io;
    mboxList *completed_parse;
    mboxWorkerPool *io_pool;
    mboxWorkerPool *parse_pool;
} mbox;

/* call back for parsing a message from a minimal representation */
static void
mboxIOMsgParseCallback(void *argv1, void *argv2)
{
    mbox *m = (mbox *)argv1;
    mboxIOMsg *msg = (mboxIOMsg *)argv2;
    mboxMsgLite *lite = mboxMsgLiteCreate(msg);
    mboxListTSAddTail(m->completed_parse, lite);
}

/* Made generic so it can work in a thread pool */
static void
mboxParserCtxGetNextMessageCallback(void *argv1, void *argv2)
{
    mbox *m = (mbox *)argv1;
    mboxParserCtx *ctx = (mboxParserCtx *)argv2;
    mboxIOMsg *msg = NULL;

    while ((msg = mboxParserCtxGetNextMessage(ctx)) != NULL) {
        mboxWorkerPoolEnqueue(m->parse_pool, mboxIOMsgParseCallback, msg);
        if (ctx->err == MBOX_PARSE_DONE) {
            break;
        }
    }
}

/* Find start of message */
static void
mboxParserCtxSeekStartCallback(void *argv1, void *argv2)
{
    (void)argv1;
    mboxParserCtx *ctx = (mboxParserCtx *)argv2;
    mboxParserCtxSeekStart(ctx);
}

mbox *
mboxReadOpen(char *file_path, int perms)
{
    struct stat st;
    int fd = -1;
    int err = 0;
    mbox *m = NULL;

    fd = open(file_path, O_RDONLY, perms);

    if (fd == -1) {
        loggerDebug("Failed to open file: %s\n", strerror(errno));
        return NULL;
    }

    err = fstat(fd, &st);

    if (err != 0) {
        loggerDebug("Failed to fstat file: %s d\n", strerror(errno));
        return NULL;
    }

    m = (mbox *)malloc(sizeof(mbox));
    m->read_refcount = 1;
    m->readfd = fd;
    loggerDebug("Fd: %d\n", fd);
    m->err = 0;
    m->file_size = st.st_size;
    m->ready = 1;

    return m;
}

static void
mboxSetAllOffsets(mbox *m)
{
    size_t io_thread_count = m->io_pool->worker_count;
    size_t chunk_size = m->file_size / io_thread_count;
    size_t non_null = 0;
    ssize_t offset = 0;
    mboxParserCtx *ctx = NULL;
    mboxParserCtx *ctx_prev = NULL;

    m->context_len = io_thread_count;
    m->contexts = (mboxParserCtx *)malloc(
            sizeof(mboxParserCtx) * io_thread_count);

    /* Find the offests for each context */
    for (size_t i = 0; i < io_thread_count; ++i) {
        offset = (ssize_t)(i * chunk_size);
        ctx = &m->contexts[i];
        mboxParserCtxInit(ctx, i, m->readfd, m->file_size);
        ctx->ioctx->fd = m->readfd;
        mboxIOSetStartOffset(ctx->ioctx, offset);
        mboxIOSetOffset(ctx->ioctx, offset);
        mboxIOSetFileOffset(ctx->ioctx, offset);
        mboxIOSetFileSize(ctx->ioctx, m->file_size);
        mboxWorkerPoolEnqueue(m->io_pool, mboxParserCtxSeekStartCallback, ctx);
    }

    mboxWorkerPoolWait(m->io_pool);

    /* Set all of the offsets */
    for (size_t i = 1; i < io_thread_count; ++i) {
        ctx = &m->contexts[i];
        ctx_prev = &m->contexts[i - 1];
        ctx_prev->ioctx->end_offset = ctx->ioctx->start_offset;
        /* reset the offset which is used for bookending messages */
        mboxIOSetOffset(ctx->ioctx, ctx->ioctx->start_offset);
    }

    /* Set the last ones offset to the length of the file */
    ctx = &m->contexts[io_thread_count - 1];
    ctx->ioctx->end_offset = ctx->ioctx->file_size;
    ctx->ioctx->offset = ctx->ioctx->start_offset;

    /* filter out and release any contexts that would have parsed nothing */
    for (size_t i = 0; i < io_thread_count; ++i) {
        ctx = &m->contexts[i];
        if (ctx->ioctx->end_offset - ctx->ioctx->start_offset <= 0) {
            continue;
        } else {
            ctx->id = non_null;
            m->read_refcount++;
            loggerDebug("[%zu]start offset: %zd\n", non_null,
                    ctx->ioctx->start_offset);
            memmove(m->contexts + non_null, &m->contexts[i],
                    sizeof(mboxParserCtx));
            non_null++;
        }
    }
    m->context_len = non_null;
}

static void
mboxParseAllMessages(mbox *m)
{
    for (size_t i = 0; i < m->context_len; ++i) {
        mboxParserCtx *ctx = &m->contexts[i];
        mboxWorkerPoolEnqueue(m->io_pool, mboxParserCtxGetNextMessageCallback,
                ctx);
    }

    mboxWorkerPoolWait(m->io_pool);
    mboxWorkerPoolWait(m->parse_pool);
}

static void
mboxParserInit(mbox *m, size_t thread_count)
{
    m->thread_count = thread_count;
    if (thread_count == 0) {
        m->ready = 1;
        return;
    }

    int io_threads = thread_count / 2;
    int parser_threads = thread_count / 2;
    m->context_len = io_threads;
    m->completed_io = mboxListNew();
    m->completed_parse = mboxListNew();
    m->io_pool = mboxWorkerPoolNew(io_threads);
    m->parse_pool = mboxWorkerPoolNew(parser_threads);
    m->ready = 1;
}

static void
mboxMain(mbox *m)
{
    mboxSetAllOffsets(m);
    mboxWorkerPoolSetPrivData(m->io_pool, m);
    mboxWorkerPoolSetPrivData(m->parse_pool, m);
    mboxParseAllMessages(m);
}

mboxList *
mboxParse(mbox *m, size_t thread_count)
{
    mboxParserInit(m, thread_count);
    mboxMain(m);
    mboxListSetFreedata(m->completed_parse,
            (mboxListFreeData *)mboxMsgLiteRelease);
    return m->completed_parse;
}

static void
mboxRemoveContext(mbox *m, int id)
{

    if (m->read_refcount == 0) {
        close(m->readfd);
    } else {
        m->read_refcount--;
    }

    if (m->write_refcount == 0) {
        close(m->writefd);
    } else {
        m->write_refcount--;
    }
    mboxBufRelease(m->contexts[id].ioctx->buf);
}

void
mboxRelease(mbox *m)
{
    mboxListRelease(m->completed_parse);
    mboxWorkerPoolRelease(m->io_pool);
    mboxWorkerPoolRelease(m->parse_pool);
    for (size_t i = 0; i < m->context_len; ++i) {
        mboxRemoveContext(m, i);
    }
    free(m->contexts);
}
