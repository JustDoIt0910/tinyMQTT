//
// Created by zr on 23-4-22.
//
#include "mqtt_buffer.h"
#include <stdio.h>

char buf[1024] = {0};

int main()
{

    tmq_buffer_t buffer;
    tmq_buffer_init(&buffer);

    tmq_buffer_append(&buffer, "asdfghjkl", 9);
    tmq_buffer_read(&buffer, buf, 9);
    printf("%s\n", buf);

    tmq_buffer_debug(&buffer);
    return 0;
}