//
// Created by zr on 23-4-18.
//
#include "mqtt_event.h"
#include "tlog.h"
#include <stdio.h>

void test_cb(tmq_socket_t fd, uint32_t event, const void* arg)
{
    printf("event\n");
}

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tmq_event_loop_t loop;
    tmq_event_loop_init(&loop);

    tmq_socket_t lis = tmq_tcp_socket();
    tmq_socket_bind(lis, "127.0.0.1", 9999);
    tmq_socket_listen(lis);

    tmq_event_handler_t* handler = tmq_event_handler_create(lis, EPOLLIN, test_cb, NULL);

    tmq_event_loop_register(&loop, handler);
    tmq_event_loop_run(&loop);

    tmq_event_loop_clean(&loop);
    tlog_exit();
    return 0;
}