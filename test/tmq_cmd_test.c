//
// Created by zr on 23-5-27.
//
#include "base/mqtt_cmd.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
    tmq_cmd_t cmd;
    tmq_cmd_init(&cmd);
    tmq_cmd_add_string(&cmd, "config", "c", "config file path", 1, "");
    tmq_cmd_add_string(&cmd, "host", "h", "host name", 1, "");
    tmq_cmd_add_number(&cmd, "port", "p", "port number", 0, 1883);
    tmq_cmd_add_bool(&cmd, "debug", "d", "debug mode");

    if(tmq_cmd_parse(&cmd, argc, argv) != -1)
    {
        tmq_str_t config = tmq_cmd_get_string(&cmd, "config");
        printf("config=%s\n", config);
        tmq_str_t host = tmq_cmd_get_string(&cmd, "host");
        printf("host=%s\n", host);
        int64_t port = tmq_cmd_get_number(&cmd, "port");
        printf("port=%ld\n", port);
    }

    tmq_cmd_destroy(&cmd);
}