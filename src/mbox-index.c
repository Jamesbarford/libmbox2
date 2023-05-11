/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <sys/types.h>

#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <unistd.h>

#include "mbox-array.h"
#include "mbox-buf.h"
#include "mbox-index.h"
#include "mbox-io.h"
#include "mbox-list.h"
#include "mbox-logger.h"
#include "mbox-memory.h"
#include "mbox-msg.h"
#include "mbox-worker.h"

typedef struct mboxIdxOffset {
    ssize_t start;
    ssize_t end;
} mboxIdxOffset;

static int
mboxListSortByStartOffset(void *d1, void *d2)
{
    ssize_t start1 = ((mboxMsgLite *)d1)->start;
    ssize_t start2 = ((mboxMsgLite *)d2)->start;
    return start1 < start2 ? -1 : start1 == start2 ? 0 : 1;
}

static int
mboxIdxTryWrite(mboxIOCtx *ioctx)
{
    ssize_t wbytes = 0;

    wbytes = mboxIOWriteBuf(ioctx, ioctx->buf->len, ioctx->file_offset);

    if (ioctx->err != MBOX_IO_OK) {
        return ioctx->err;
    }

    mboxIOFsync(ioctx);

    if (ioctx->err != MBOX_IO_OK) {
        return ioctx->err;
    }

    ioctx->file_offset += wbytes;
    ioctx->file_size += wbytes;
    mboxBufSetLen(ioctx->buf, 0);
    mboxBufSetOffset(ioctx->buf, 0);

    return MBOX_IO_OK;
}

/* Save a linked list of lite messages to a simple file format that will allow
 * quickly loading in all of the messages again without having to scan the raw
 * mbox file to find all off the messages
 *
 * 0 3000\r\n // this is what is persisted to a file
 * */
int
mboxIdxSave(char *filename, mboxList *l)
{
    mboxIOCtx *ioctx = NULL;
    mboxLNode *node = NULL;
    mboxMsgLite *msg = NULL;
    int err = MBOX_IO_OK;
    ssize_t wbytes = 0;
    size_t count = 0;
    char startstr[26];
    char endstr[26];

    /* No need to do anything */
    if (l->len == 0) {
        return 0;
    }

    ioctx = mboxIOOpen(filename, O_RDWR | O_TRUNC | O_CREAT, 0666);

    if (ioctx == NULL) {
        loggerDebug("Failed to open\n");
        return 0;
    }

    mboxListQSort(l, mboxListSortByStartOffset);
    count = l->len;
    node = l->root;

    do {
        msg = node->data;

        mboxBufCatPrintf(ioctx->buf, "%zu %zu\n", msg->start, msg->end);

        /* Occassionally do a write */
        if (ioctx->buf->len % BUFSIZ == 0) {
            if (mboxIdxTryWrite(ioctx) != MBOX_IO_OK) {
                err = ioctx->err;
                goto out;
            }
        }

        node = node->next;
        count--;
    } while (count > 0);

    if (ioctx->buf->len > 0) {
        if (mboxIdxTryWrite(ioctx) != MBOX_IO_OK) {
            err = ioctx->err;
            goto out;
        }
    }

out:
    mboxIOClose(ioctx);
    return err;
}

/* counts how many lines are in an index file which will broadly tell us how
 * many offsets there are */
size_t
mboxIdxLineCount(mboxIOCtx *ioctx)
{
    size_t count = 0;
    for (size_t i = 0; i < ioctx->buf->len; ++i) {
        if (ioctx->buf->data[i] == '\n') {
            count++;
        }
    }
    return count;
}

/* Read an index file into memory */
static mboxIOCtx *
loadIndexes(char *idxfile)
{
    mboxIOCtx *ioctx = mboxIOOpen(idxfile, O_RDONLY, 0666);

    if (ioctx == NULL) {
        loggerDebug("No idx file\n");
        /* file does not exist */
        return NULL;
    }

    /* Try and read the whole file into the buffer, should be do-able as it is
     * so small */
    if (mboxIORead(ioctx, ioctx->file_size, 0, 0) != ioctx->file_size) {
        loggerDebug("Failed to read in whole indexFile\n");
        mboxIOClose(ioctx);
        return NULL;
    }

    return ioctx;
}

/* int * saving the start and end as a tuple, or 2 integer array.
 * Takes the memory buffer form of the index list and converts it
 * to a list.
 * */
static mboxList *
indexesToList(mboxIOCtx *ioctx)
{
    mboxList *l = mboxListNew();
    char tmpstart[26], tmpend[26];
    int startidx = 0, endidx = 0;
    mboxBuf *buf = ioctx->buf;

    size_t count = mboxIdxLineCount(ioctx);
    //    mboxArray *array = mboxArrayNew(sizeof(mboxIdxOffset), count);
    //    mboxIdxOffset off;

    mboxListSetFreedata(l, free);

    while (buf->offset < buf->len) {
        startidx = endidx = 0;

        while (buf->data[buf->offset] != ' ') {
            tmpstart[startidx++] = buf->data[buf->offset++];
        }

        buf->offset++;

        while (buf->data[buf->offset] != '\n') {
            tmpend[endidx++] = buf->data[buf->offset++];
        }

        tmpstart[startidx] = '\0';
        tmpend[endidx] = '\0';

        size_t *offsets = (size_t *)malloc(sizeof(ssize_t) * 2);
        offsets[0] = (ssize_t)atol(tmpstart);
        offsets[1] = (ssize_t)atol(tmpend);

        //       mboxArrayPush(array, &off);
        mboxListAddTail(l, offsets);
        buf->offset++;
    }

    return l;
}

typedef struct mboxIdxCtx {
    int mbox_fd;
    mboxList *msgs;
} mboxIdxCtx;

typedef struct mboxIdxMsgCtx {
    mboxBuf *buf;
    size_t bytes_to_read;
    size_t diff;
    size_t count;
} mboxIdxMsgCtx;

void
mboxIdxGetMessages(void *privdata, void *data)
{
    mboxIdxCtx *idxctx = (mboxIdxCtx *)privdata;
    mboxList *batch = (mboxList *)data;

    /* This is so we can translate the offsets to our buffer */
    ssize_t diff = ((ssize_t *)batch->root->data)[0];
    /* Figure out how much we need to read */
    ssize_t size = ((ssize_t *)batch->root->prev->data)[1] - diff;
    /* For housing start and end offsets */
    ssize_t *offset = NULL;

    mboxChar *buf = malloc((sizeof(mboxChar) * size) + 10);

    mboxBuf tmp;

    /* Read the entire thing into a buffer */
    ssize_t rbytes = pread(idxctx->mbox_fd, buf, size, diff);
    buf[rbytes] = '\0';

    do {
        /* DO NOT FREE THE OFFSET, we take care of all of them later. This is
         * just a clone */
        offset = mboxListRemoveHead(batch);

        /*
        if (offset == NULL) {
            loggerDebug("No offset\n");
        }
        */

        /* As all of our parsing works on offsets we can reuse the same buffer
         * and reset the offsets for each call to get the msglite */
        tmp.data = buf + (offset[0] - diff);
        tmp.len = offset[1] - offset[0];
        tmp.offset = 0;
        tmp.capacity = 0;
        mboxMsgLite *msg = mboxMsgLiteFromBuffer(&tmp, offset[0], offset[1]);

        if (msg) {
            mboxListTSAddTail(idxctx->msgs, msg);
        }
    } while (batch->len > 0);

    free(buf);
    mboxListRelease(batch);
}

/* ioctx is a handle on the mbox file we are spamming malloc and free */
void
mboxIdxBatchLoad(mboxIdxCtx *idxctx, mboxList *indexes, int thread_count)
{
    mboxList *batch = mboxListNew();
    mboxWorkerPool *pool = mboxWorkerPoolNew(thread_count);
    ssize_t *offsets = NULL;
    size_t batch_bytes = 0;
    size_t message_size = 0;

    mboxWorkerPoolSetPrivData(pool, idxctx);

    do {
        offsets = (ssize_t *)mboxListRemoveHead(indexes);

        message_size = offsets[1] - offsets[0];

        mboxListAddTail(batch, offsets);

        if (batch_bytes + message_size < MBOX_IO_READ_SIZE) {
            batch_bytes += message_size;
        } else {
            /* Spawn threadpool task */
            mboxWorkerPoolEnqueue(pool, mboxIdxGetMessages, batch);
            batch_bytes = 0;
            batch = mboxListNew();
        }
    } while (indexes->len > 0);

    if (batch->len > 0) {
        mboxWorkerPoolEnqueue(pool, mboxIdxGetMessages, batch);
    }

    mboxWorkerPoolWait(pool);
}

/* Load mboxLiteMsg from file indexes and parse based on offsets */
mboxList *
mboxIdxLoad(char *idxfile, char *mboxfile, unsigned int thread_count)
{
    mboxList *indexes = NULL;
    mboxList *msgs = NULL;
    mboxIOCtx *ioidx = NULL;
    mboxIdxCtx *idxctx = NULL;

    ioidx = loadIndexes(idxfile);

    if (ioidx == NULL) {
        loggerDebug("No idx file\n");
        /* file does not exist */
        return NULL;
    }

    msgs = mboxListNew();
    mboxListSetFreedata(msgs, (mboxListFreeData *)mboxMsgLiteRelease);

    indexes = indexesToList(ioidx);

    mboxIOClose(ioidx);

    idxctx = (mboxIdxCtx *)malloc(sizeof(mboxIdxCtx));
    idxctx->msgs = msgs;
    idxctx->mbox_fd = open(mboxfile, O_RDONLY, 0666);

    printf("index count: %zu\n", indexes->len);
    mboxIdxBatchLoad(idxctx, indexes, thread_count);

    close(idxctx->mbox_fd);
    mboxListRelease(indexes);
    free(idxctx);

    return msgs;
}
