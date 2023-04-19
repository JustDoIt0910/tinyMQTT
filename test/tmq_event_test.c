//
// Created by zr on 23-4-18.
//
#include "mqtt_event.h"
#include "mqtt_acceptor.h"
#include "tlog.h"
#include <stdio.h>

void new_conn(tmq_socket_t conn, tmq_socket_addr_t* addr, const void* arg)
{
    printf("new_conn\n");
    tmq_event_loop_t* loop = (tmq_event_loop_t*)arg;
    tmq_event_loop_quit(loop);
}

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tmq_event_loop_t loop;
    tmq_event_loop_init(&loop);

    tmq_acceptor_t acceptor;
    tmq_acceptor_init(&acceptor, &loop, &loop, 9999);
    tmq_acceptor_set_cb(&acceptor, new_conn);
    tmq_acceptor_listen(&acceptor);

    tmq_event_loop_run(&loop);

    tmq_event_loop_clean(&loop);
    tlog_exit();
    return 0;
}