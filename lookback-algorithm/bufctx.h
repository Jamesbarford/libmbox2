#ifndef __BUFCTX_H
#define __BUFCTX_H

#include <sys/types.h>

#include <stddef.h>

typedef struct bufCtx {
    /* If this is true after a thread has completed then there is a race
     * condition and there will be a program crash. Run 'try-segfault.sh' to
     * test */
    int debug_has_freed; /* See if the struct has been freed by the caller */

    int id;               /* Id of context */
    int fd;               /* File descriptor */
    int err;              /* Error code if any */
    size_t file_size;     /* Size of the file we are reading */
    ssize_t start_offset; /* Where we intially start in the file, will get
                             updated to find the nearest pattern */
    ssize_t end_offset;  /* Where we will end, initially set to -1 meaning unset
                          */
    ssize_t file_offset; /* Where we currently are in the file */

    char *buf;           /* Buffer */
    size_t buflen;       /* Number of bytes in buffer */
    size_t buf_offset;   /* Offset of the buffer */
    size_t buf_capacity; /* The current capacity of the buffer */
} bufCtx;

void bufCtxInit(bufCtx *ctx, int id, int fd, size_t file_size);
bufCtx *bufCtxNew(int id, int fd, size_t file_size);
ssize_t bufCtxRead(bufCtx *ctx, int buf_offset, int file_offset);
void bufCtxCollectFullBuffer(bufCtx *ctx);
void bufCtxSeekStart(bufCtx *ctx);
void bufCtxRelease(bufCtx *ctx);

#endif
