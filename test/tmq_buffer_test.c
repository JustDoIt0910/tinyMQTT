//
// Created by zr on 23-4-22.
//
#include "net/mqtt_buffer.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "tlog.h"
#include <stdio.h>

char buf[65536] = {0};
char buf2[65536] = {0};

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);
    tmq_buffer_t buffer;
    tmq_buffer_init(&buffer);

    int fd = open("../../mqtt_buffer.c", O_RDONLY);

    read(fd, buf, 100);
    tmq_buffer_append(&buffer, buf, 100);

    read(fd, buf, 600);
    tmq_buffer_append(&buffer, buf, 600);

    read(fd, buf, 1024);
    tmq_buffer_append(&buffer, buf, 1024);

    read(fd, buf, 1024);
    tmq_buffer_append(&buffer, buf, 1024);

    read(fd, buf, 4096);
    tmq_buffer_append(&buffer, buf, 4096);


    tmq_buffer_read(&buffer, buf2, 3000);

    read(fd, buf, 600);
    tmq_buffer_append(&buffer, buf, 600);

    read(fd, buf, 200);
    tmq_buffer_append(&buffer, buf, 200);

    bzero(buf2, sizeof(buf2));
    tmq_buffer_read(&buffer, buf2, buffer.readable_bytes);
//    printf("%s\n\n", buf2);
//    printf("%ld\n", strlen(buf2));

    tmq_buffer_read_fd(&buffer, fd, 5100);

    bzero(buf2, sizeof(buf2));
    tmq_buffer_peek(&buffer, buf2, buffer.readable_bytes);
    printf("%s\n\n", buf2);
    printf("%ld\n", strlen(buf2));

    tmq_buffer_debug(&buffer);
    tlog_exit();
    return 0;
}