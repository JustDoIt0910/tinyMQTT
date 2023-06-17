//
// Created by zr on 23-6-18.
//
#include "event/mqtt_event.h"
#include "net/mqtt_connector.h"
#include "tlog.h"
#include <stdio.h>

void on_connect(tmq_socket_t sock)
{
    printf("connected\n");
}

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tmq_event_loop_t loop;
    tmq_event_loop_init(&loop);

    tmq_connector_t connector;
    tmq_connector_init(&connector, &loop, "127.0.0.1", 9999, on_connect, 5);
    tmq_connector_connect(&connector);

    tmq_event_loop_run(&loop);
    tmq_event_loop_destroy(&loop);
    tlog_exit();
    return 0;
}