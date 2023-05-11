#ifndef __IO_H
#define __IO_H

#include <sys/stat.h>

#include <stddef.h>
#include <stdio.h>

#ifndef BUF_READ_SIZE
#define BUF_READ_SIZE (6)
#endif

#define EXAMPLE_LEN (40)
#define _O_RDONLY (0)

struct _stat {
    ino_t st_ino;
    off_t st_size;
};

struct _file {
    char *path;    /* path of the file */
    ino_t f_inode; /* inode number of this file  */
    long f_count; /* An atomic long integer that counts the number of references
                     to this file descriptor entry.*/
    unsigned int f_flags; /* A bitfield that contains various flags associated
                             with the file descriptor.*/
    long f_mode; /* The file access mode and options, such as read, write, or
                    both. */
    off_t f_pos; /* Offset of file */
};

ssize_t __pread(int fd, void *buf, size_t size, off_t offset);
ssize_t __read(int fd, void *buf, size_t size);
int __open(char *path, int oflag, int perms);
int __fstat(int fd, struct _stat *s);

void ioClear(void);
void ioInit(void);

#endif
