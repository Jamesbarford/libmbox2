/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __MBOXIO_H
#define __MBOXIO_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "mbox-buf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBOX_IO_FSYNC_ERR (-5)
#define MBOX_IO_NO_WRITE (-4)
#define MBOX_IO_WRITE_ERR (-3)
#define MBOX_IO_EOF (-2)
#define MBOX_IO_READ_ERR (-1)
#define MBOX_IO_OK (0)
#define MBOX_IO_DONE (1)

#define MBOX_IO_READ_SIZE (300000)

typedef struct mboxIOCtx {
    int fd;               /* File descriptor */
    int err;              /* Error code '0' is all good */
    size_t offset;        /* Offset in the overall file, we use this too bookend
                             messages */
    ssize_t start_offset; /* Where we intially start in the file, will get
                             updated to find the nearest pattern */
    ssize_t end_offset;  /* Where we will end, initially set to -1 meaning unset
                          */
    ssize_t file_offset; /* Where we currently are in the file */
    size_t file_size;    /* Size of the file */
    mboxBuf *buf;        /* Buffer for reading into */
} mboxIOCtx;

#define mboxIOSetFd(io, _fd) ((io)->fd = (_fd))
#define mboxIOSetStartOffset(io, off) ((io)->start_offset = (off))
#define mboxIOSetEndOffset(io, off) ((io)->end_offset = (off))
#define mboxIOSetOffset(io, off) ((io)->offset = (off))
#define mboxIOSetFileOffset(io, off) ((io)->file_offset = (off))
#define mboxIOSetFileSize(io, size) ((io)->file_size = (size))

mboxIOCtx *mboxIONew(int fd, size_t file_size);
mboxIOCtx *mboxIOOpen(char *path, int perms, int mode);
void mboxIOClose(mboxIOCtx *ioctx);
int mboxIOExists(char *path);

/* Read from fd into buffer, given how much to read 'size' a buffer offset of
 * where to read and where in the file to read from with file_offset */
ssize_t mboxIORead(mboxIOCtx *ioctx, size_t size, size_t buf_offset,
        ssize_t offset);

ssize_t mboxIOWriteBuf(mboxIOCtx *ioctx, size_t size, ssize_t offset);
int mboxIOFsync(mboxIOCtx *ioctx);

/* Loose wrapper over pwrite syscall */
ssize_t mboxIOWrite(mboxIOCtx *ioctx, void *buf, size_t size, ssize_t offset);

#ifdef __cplusplus
}
#endif

#endif
