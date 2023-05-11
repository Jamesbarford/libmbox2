#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct fileCtx {
    int fd;
    size_t file_size;
    size_t buflen;
    size_t bufoff;
    char *buf;
} fileCtx;

void
panic(char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    fprintf(stderr, "%s", buf);
    va_end(ap);
    exit(EXIT_FAILURE);
}

fileCtx *
fileOpen(char *path)
{
    int fd;
    struct stat st;
    fileCtx *ctx;
    char *buf;

    if ((fd = open(path, O_RDWR, 0666)) == -1) {
        panic("Failed to open file: %s\n", strerror(errno));
    }

    if (fstat(fd, &st) == -1) {
        panic("Failed to fstat file: %s\n", strerror(errno));
    }

    buf = malloc((sizeof(char) * st.st_size) + 1);
    if (read(fd, buf, st.st_size) != st.st_size) {
        panic("Failed to read full file: %s\n", strerror(errno));
    }

    buf[st.st_size] = '\0';
    ctx = malloc(sizeof(fileCtx));

    ctx->file_size = st.st_size;
    ctx->buf = buf;
    ctx->buflen = st.st_size;
    ctx->bufoff = 0;
    ctx->fd = fd;

    return ctx;
}

/* This will need a lock as it is very destructive */
void
fileRemoveFrom(fileCtx *ctx, off_t start_offset, off_t end_offset)
{
    char buf[BUFSIZ];
    size_t file_size = ctx->file_size;
    ssize_t size = end_offset - start_offset;
    size_t new_size = file_size - size;
    ssize_t rbytes = 0;
    ssize_t wbytes = 0;

    while ((rbytes = pread(ctx->fd, buf, sizeof(buf), end_offset)) > 0) {
        wbytes = pwrite(ctx->fd, buf, rbytes, start_offset);
        end_offset += rbytes;
        start_offset += wbytes;
    }

    ftruncate(ctx->fd, new_size);
}

int
main(void)
{
    fileCtx *ctx = fileOpen("./sample.txt");
    fileRemoveFrom(ctx, 27, 59);

    // int start = 10;
    // int end = 100;
}
