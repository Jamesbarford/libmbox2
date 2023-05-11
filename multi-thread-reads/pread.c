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

int
main(void)
{
    int fd = open("./main.c", O_RDONLY, 0666);
    char buf[10];
    pread(fd, buf, sizeof(buf), 100);
    printf("%s\n", buf);
    pread(fd, buf, sizeof(buf), 101);
    printf("%s\n", buf);
    pread(fd, buf, sizeof(buf), 102);
    printf("%s\n", buf);
    pread(fd, buf, sizeof(buf), 103);
    printf("%s\n", buf);
}
