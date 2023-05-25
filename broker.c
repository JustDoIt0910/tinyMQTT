//
// Created by zr on 23-4-9.
//
#include "tlog.h"
#include "mqtt/mqtt_broker.h"
#include <stdio.h>

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);
    printf("   __                            __  ___   ____   ______  ______\n"
           "  / /_   (_)   ____    __  __   /  |/  /  / __ \\ /_  __/ /_  __/\n"
           " / __/  / /   / __ \\  / / / /  / /|_/ /  / / / /  / /     / /   \n"
           "/ /__  / /   / / / / / /_/ /  / /  / /  / /_/ /  / /     / /    \n"
           "\\__/  /_/   /_/ /_/  \\___ /  /_/  /_/   \\___\\_\\ /_/     /_/     \n"
           "                    /____/                                      \n");

    tmq_broker_t broker;
    if(tmq_broker_init(&broker, "tinymqtt.conf") == 0)
        tmq_broker_run(&broker);

    tlog_exit();
    return 0;
}