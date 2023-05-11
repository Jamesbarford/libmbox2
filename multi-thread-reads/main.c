#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef THREAD_COUNT
#define THREAD_COUNT (2)
#endif

#ifndef BUF_READ_SIZE
#define BUF_READ_SIZE (20)
#endif

#define BUF_CAPACITY                                                        \
    (10000) /* This is comically large so there is no realloc logic in this \
               simple example */

#define loggerDebug(...)                                                   \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
    } while (0)

typedef struct fileCtx {
    int id;                  /* Id of ctx */
    int fd;                  /* File descriptor */
    off_t file_start_offset; /* File offset to read from */
    off_t file_end_offset;   /* File offset to end at */
    off_t file_offset;       /* Where we actually are */
    size_t chunk_size;   /* The chunk size this work should be dealing with */
    size_t buflen;       /* How many bytes read in the buffer */
    size_t buf_capacity; /* Capacity of buffer */
    size_t buf_offset;   /* Offset in the buffer */
    char *buf;           /* Buffer for reading */
    int err;             /* 0 means we're ok, anything else is an error */
    pthread_t th;        /* Thread */
} fileCtx;

void
panic(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];

    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    buf[len] = '\0';
    fprintf(stderr, "%s", buf);
    va_end(ap);
    exit(1);
}

int
fileCtxRead(fileCtx *ctx)
{
    ssize_t read_bytes = 0;
    /* By using pread we will not interfere with the file descriptors offset
     * pointer */
    read_bytes = pread(ctx->fd, ctx->buf + ctx->buflen, BUF_READ_SIZE,
            ctx->file_offset);

    if (read_bytes == -1) {
        ctx->err = errno;
        return ctx->err;
    }

    if (read_bytes == 0) {
        ctx->err = EOF;
        return EOF;
    }

    ctx->buflen += read_bytes;
    ctx->file_offset += read_bytes;
    ctx->buf[ctx->buflen] = '\0';
    return 1;
}

ssize_t
fileCtxFillBuffer(fileCtx *ctx)
{
    ssize_t rbytes = pread(ctx->fd, ctx->buf, BUF_READ_SIZE,
            ctx->file_start_offset);
    if (rbytes == -1) {
        ctx->err = errno;
        return ctx->err;
    }

    if (rbytes == 0) {
        ctx->err = EOF;
        return ctx->err;
    }

    ctx->buf[ctx->buflen] = '\0';
    return rbytes;
}

void
fileCtxInit(fileCtx *ctx, int fd)
{
    ctx->fd = fd;
    ctx->buflen = 0;
    ctx->buf_capacity = 1000;
    ctx->buf = malloc(sizeof(char) * ctx->buf_capacity);
}

int
isFromLine(const char *s)
{
    if (s == NULL) {
        return 0;
    }
    if (s[0] == '\0' || s[1] == '\0' || s[2] == '\0' || s[3] == '\0' ||
            s[4] == '\0') {
        return 0;
    }
    int match = s[0] == 'F' && s[1] == 'r' && s[2] == 'o' && s[3] == 'm' &&
            s[4] == ' ';
    if (match == 1) {
        loggerDebug("%.*s\n", 5, s);
    }
    return match == 1;
}

/**
 * From the current offest find the nearest From line
 */
int
fileCtxSeekFromLine(fileCtx *ctx)
{
    while (1) {
        if (ctx->buf_offset + 4 >= ctx->buflen) {
            if (fileCtxRead(ctx) != 1) {
                break;
            }
        }

        if (isFromLine(ctx->buf + ctx->buf_offset)) {
            return 1;
        }

        ctx->buf_offset++;
    }

    return 0;
}

void *
fileCtxDoWork(void *argv)
{
    fileCtx *ctx = (fileCtx *)argv;
    ctx->file_offset = ctx->file_start_offset;

    if (fileCtxRead(ctx) != 1) {
        return NULL;
    }

    if (fileCtxSeekFromLine(ctx) != 1) {
        ctx->err = 1;
        loggerDebug("FAIL: %d\n", ctx->id);
        return NULL;
    }
    /* We do not null terminate this string as we are going to do some more
     * parsing :D */
    size_t newlen = ctx->buflen - ctx->buf_offset;
    memmove(ctx->buf, ctx->buf + ctx->buf_offset, newlen);
    ctx->buflen = newlen;

    /* We're pointing at the begining of a from line, moving the offset to that
     * start */
    ctx->file_start_offset += ctx->buf_offset;

    /* We want to start reading passed the current from */
    ctx->file_offset = ctx->file_start_offset + ctx->buflen;
    ctx->buf_offset = ctx->buflen;

    while (1) {
        if (fileCtxSeekFromLine(ctx) == 1 || ctx->err == EOF) {
            break;
        }
    }

    /* We have more buffer that we can proccess, this ensures that we parse
     * everything */
    if (ctx->file_offset < ctx->file_end_offset) {
        ctx->buf_offset++;
        while (ctx->file_offset < ctx->file_end_offset) {
            if (fileCtxSeekFromLine(ctx) == 1 || ctx->err == EOF) {
                /* We only want ot break if there is nothing more to read or our
                 * work is done */
                if (ctx->err == EOF ||
                        ctx->file_offset >= ctx->file_end_offset) {
                    break;
                }
                /* Continue to the next part */
                ctx->buf_offset++;
            }
        }
    }

    if (ctx->err == EOF) {
        /* As our check in SeekFromLine is + 4*/
        ctx->buf_offset += 4;
    }
    ctx->buflen = ctx->buf_offset;
    ctx->buf[ctx->buf_offset] = '\0';
    ctx->err = 0;

    return NULL;
}

int
main(int argc, char **argv)
{
    if (argc != 2) {
        panic("Usage: %s <file>\n", argv[0]);
    }

    printf("THREAD_COUNT  => %d\nBUF_READ_SIZE => %d\n", THREAD_COUNT,
            BUF_READ_SIZE);

    char *path = argv[1];
    int fd = open(path, O_RDONLY, 0666);
    size_t len = 0;
    size_t chunk_size = 0;
    struct stat sb;
    fileCtx *contexts = NULL;

    if (fd == -1) {
        panic("Unable to open file: %s\n", path);
    }

    if (fstat(fd, &sb) != 0) {
        panic("failed to fstat file: %s", strerror(errno));
    }

    len = sb.st_size;
    chunk_size = (sb.st_size / THREAD_COUNT);
    contexts = malloc(sizeof(fileCtx) * THREAD_COUNT);

    printf("CHUNK SIZE: %zu\n", chunk_size);

    for (int i = 0; i < THREAD_COUNT; ++i) {
        fileCtx *ctx = &contexts[i];
        fileCtxInit(ctx, fd);
        ctx->id = i;
        ctx->file_start_offset = i * chunk_size;
        ctx->chunk_size = chunk_size;
        ctx->file_end_offset = sb.st_size <
                        ctx->file_start_offset + chunk_size ?
                sb.st_size :
                ctx->file_start_offset + chunk_size;
        pthread_create(&ctx->th, NULL, fileCtxDoWork, ctx);
    }

    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_join(contexts[i].th, NULL);
    }

    for (int i = 0; i < THREAD_COUNT; ++i) {
        fileCtx *ctx = &contexts[i];
        if (ctx->err == 0) {
            loggerDebug("-----------thread: %d-----------\n", ctx->id);

            printf("%.*s\n", 100, ctx->buf);
            printf("offset: %lld %lld\n", ctx->file_start_offset,
                    ctx->file_end_offset);
        }
        free(ctx->buf);
    }

    free(contexts);
    close(fd);
}
