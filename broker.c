//
// Created by zr on 23-4-9.
//
#include "tlog.h"
#include "mqtt/mqtt_broker.h"

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tmq_broker_t broker;
    tmq_broker_init(&broker, 9999);
    tmq_broker_run(&broker);

    tlog_exit();
    return 0;
}