//
// Created by zr on 23-4-22.
//
#include "mqtt_buffer.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

char buf[1024] = {0};
char buf2[65536] = {0};

int main()
{

    tmq_buffer_t buffer;
    tmq_buffer_init(&buffer);

    int fd = open("../../mqtt_buffer.c", O_RDONLY);

    read(fd, buf, 100);
    tmq_buffer_append(&buffer, buf, 100);

    read(fd, buf, 600);
    tmq_buffer_append(&buffer, buf, 600);

    read(fd, buf, 1024);
    tmq_buffer_append(&buffer, buf, 1024);


    tmq_buffer_read(&buffer, buf2, 1724);
    printf("%s\n", buf2);

    tmq_buffer_debug(&buffer);
    return 0;
}