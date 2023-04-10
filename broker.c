//
// Created by zr on 23-4-9.
//
#include "tlog.h"
#include <stdio.h>
#include "mqtt_socket.h"

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);
    tmq_tcp_socket();
    tlog_exit();
    return 0;
}