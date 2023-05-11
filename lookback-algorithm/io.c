#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"

/* stdin, stdout, stderr ... hence '3'*/
static int __current_fd = 3;
static struct _file *file_table[100];
static const int file_table_size = sizeof(file_table) / sizeof(file_table[0]);

/* File avalible on our system */
static const char *path_name = "example.txt";
static const char EXAMPLE[EXAMPLE_LEN] =
        "AA~~~~~~AA------AA___AA:::::::AA>>>>>>\0";

int
__open(char *path, int oflag, int perms)
{
    (void)perms;
    if (strncmp(path, path_name, 11) != 0) {
        return -1;
    }

    if (__current_fd == file_table_size) {
        /* table is full */
        return -1;
    }

    struct _file *f = malloc(sizeof(struct _file));
    f->path = strdup(path);
    f->f_inode = 1;
    f->f_flags = 0;
    f->f_pos = 0;
    f->f_mode = oflag;
    f->f_count = 1;

    file_table[__current_fd] = f;

    int fd = __current_fd;
    __current_fd++;

    return fd;
}

ssize_t
__pread(int fd, void *buf, size_t nbyte, off_t offset)
{
    struct _file *f = file_table[fd];
    ssize_t rbytes = 0;
    ssize_t to_read = nbyte > INT_MAX ? INT_MAX : nbyte;

    if (!f) {
        return -1;
    }

    for (; rbytes < to_read; ++rbytes) {
        if (offset + rbytes > EXAMPLE_LEN) {
            errno = EOF;
            break;
        }
        ((char *)buf)[rbytes] = EXAMPLE[offset + rbytes];
    }

    return rbytes;
}

int
__fstat(int fd, struct _stat *s)
{
    struct _file *f = file_table[fd];

    if (!f) {
        return -1;
    }

    s->st_ino = 0;
    s->st_size = EXAMPLE_LEN;
    return 0;
}

void
ioClear(void)
{
    for (int i = 3; i < __current_fd; ++i) {
        struct _file *f = file_table[i];
        free(f->path);
        free(f);
        file_table[i] = NULL;
    }
}

void
ioInit(void)
{
    for (int i = 3; i < file_table_size; ++i) {
        file_table[i] = NULL;
    }
}
