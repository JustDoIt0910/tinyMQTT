//
// Created by zr on 23-4-9.
//
#include "tlog.h"
#include "mqtt/mqtt_broker.h"
#include "base/mqtt_cmd.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);
    printf("   __                            __  ___   ____   ______  ______\n"
           "  / /_   (_)   ____    __  __   /  |/  /  / __ \\ /_  __/ /_  __/\n"
           " / __/  / /   / __ \\  / / / /  / /|_/ /  / / / /  / /     / /   \n"
           "/ /__  / /   / / / / / /_/ /  / /  / /  / /_/ /  / /     / /    \n"
           "\\__/  /_/   /_/ /_/  \\___ /  /_/  /_/   \\___\\_\\ /_/     /_/     \n"
           "                    /____/                                      \n");
    tmq_cmd_t cmd;
    tmq_cmd_init(&cmd);
    tmq_cmd_add_number(&cmd, "port", "p", "server port", 0, 1883);
    tmq_cmd_add_number(&cmd, "cluster-port", "c", "cluster port", 0, 11883);
    if(tmq_cmd_parse(&cmd, argc, argv) < 0)
    {
        tmq_cmd_destroy(&cmd);
        tlog_exit();
        return 0;
    }
    tmq_broker_t broker;
    if(tmq_broker_init(&broker, "tinymqtt.conf", &cmd) == 0)
        tmq_broker_run(&broker);
    tlog_exit();
    return 0;
}