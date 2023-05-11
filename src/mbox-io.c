/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mbox-buf.h"
#include "mbox-io.h"
#include "mbox-logger.h"

mboxIOCtx *
mboxIONew(int fd, size_t file_size)
{
    mboxIOCtx *ioctx = (mboxIOCtx *)malloc(sizeof(mboxIOCtx));
    ioctx->file_offset = 0;
    ioctx->offset = 0;
    ioctx->end_offset = 0;
    ioctx->start_offset = 0;
    ioctx->fd = fd;
    ioctx->err = MBOX_IO_OK;
    ioctx->file_size = file_size;
    ioctx->buf = mboxBufAlloc(MBOX_IO_READ_SIZE);
    return ioctx;
}

void
mboxIORelease(mboxIOCtx *ioctx)
{
    if (ioctx) {
        mboxBufRelease(ioctx->buf);
        free(ioctx);
    }
}

int
mboxIOExists(char *path)
{
    int ok = access(path, F_OK);
    return ok != -1;
}

void
mboxIOClose(mboxIOCtx *ioctx)
{
    if (ioctx) {
        if (ioctx->fd != -1) {
            close(ioctx->fd);
            mboxIORelease(ioctx);
        }
    }
}

mboxIOCtx *
mboxIOOpen(char *name, int perms, int mode)
{

    int fd = open(name, perms, mode);
    mboxIOCtx *ioctx = NULL;
    struct stat st;

    if (fd == -1) {
        printf("hy2?\n");
        return NULL;
    }

    if (fstat(fd, &st) == -1) {
        return NULL;
    }

    ioctx = mboxIONew(fd, st.st_size);

    return ioctx;
}

/* Read from fd into buffer, given how much to read 'size' a buffer offset of
 * where to read and where in the file to read from with file_offset */
ssize_t
mboxIORead(mboxIOCtx *ioctx, size_t size, size_t buf_offset, ssize_t offset)
{
    mboxBufExtendBufferIfNeeded(ioctx->buf, size);

    ssize_t rbytes = 0;
    ioctx->err = MBOX_IO_OK;
    /* We are potentially going to have multiple threads smashing this file
     * descriptor and going to be hopping all over the place to it makes sense
     * to use pread */
    rbytes = pread(ioctx->fd, ioctx->buf->data + buf_offset, size, offset);

    if (rbytes == 0) {
        loggerDebug("NO READ, file offset: %zu\n", ioctx->file_offset);
        ioctx->err = MBOX_IO_EOF;
    } else if (rbytes < 0) {
        /* TODO: Better error handling */
        ioctx->err = MBOX_IO_READ_ERR;
    } else {
        mboxBufSetLen(ioctx->buf, buf_offset + rbytes);
    }

    return rbytes;
}

ssize_t
mboxIOWrite(mboxIOCtx *ioctx, void *buf, size_t size, ssize_t offset)
{
    ssize_t wbytes = pwrite(ioctx->fd, buf, size, offset);

    if (wbytes == 0) {
        loggerDebug("No write: %zu\n", offset);
        ioctx->err = MBOX_IO_NO_WRITE;
    } else if (wbytes < 0) {
        ioctx->err = MBOX_IO_WRITE_ERR;
    }

    return wbytes;
}

ssize_t
mboxIOWriteBuf(mboxIOCtx *ioctx, size_t size, ssize_t offset)
{
    return mboxIOWrite(ioctx, ioctx->buf->data, size, offset);
}

int
mboxIOFsync(mboxIOCtx *ioctx)
{
    ioctx->err = MBOX_IO_OK;
    int err = fsync(ioctx->fd);

    if (err != 0) {
        ioctx->err = MBOX_IO_FSYNC_ERR;
    }

    return ioctx->err;
}
