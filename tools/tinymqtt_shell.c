//
// Created by just do it on 2024/1/18.
//
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include "mqtt_console_cmd.h"
#include "codec/mqtt_console_codec.h"
#include "net/mqtt_socket.h"

int connect_to_broker()
{
    int fd = tmq_unix_socket(0);
    if(tmq_unix_socket_connect(fd, "/tmp/tinymqtt_console.path") < 0)
    {
        tmq_socket_close(fd);
        return -1;
    }
    return fd;
}

void add_new_user(args_map* args, void* context)
{
    char** user = tmq_map_get(*args, "username");
    char** pwd = tmq_map_get(*args, "password");
    int fd = *(int*)context;
    if(send_add_user_message(fd, *user, *pwd) < 0 || receive_user_operation_reply(fd) < 0)
    {
        printf("connection closed\n");
        exit(0);
    }
}

void del_user(args_map* args, void* context)
{
    printf("not implemented\n");
}

void change_user_pwd(args_map* args, void* context)
{
    printf("not implemented\n");
}

void add_acl_rule(args_map* args, void* context)
{
    printf("not implemented\n");
}

void list_acl_rules(args_map* args, void* context)
{
    printf("not implemented\n");
}

void list_subscription_of_user(args_map* args, void* context)
{
    printf("not implemented\n");
}

void quit(args_map* args, void* context){ exit(0);}

void init_commands(tmq_console_cmd_t* cmd)
{
    tmq_console_cmd_init(cmd);

    CONSOLE_COMMAND(cmd, add_new_user, "add", "user", CONSOLE_VARIABLE("username"), CONSOLE_VARIABLE("password"));
    CONSOLE_COMMAND(cmd, del_user, "del", "user", CONSOLE_VARIABLE("username"));
    CONSOLE_COMMAND(cmd, change_user_pwd, "change", "password", "of", CONSOLE_VARIABLE("username"),
                    CONSOLE_VARIABLE("new_password"));
    CONSOLE_COMMAND(cmd, add_acl_rule,
                    CONSOLE_OPTION("permission", "allow", "deny"),
                    CONSOLE_OPTION("rule_type", "user", "client_id", "ip"),
                    CONSOLE_VARIABLE("who"),
                    CONSOLE_OPTION("access", "sub", "pub", "sub/pub"),
                    CONSOLE_VARIABLE("topic"));
    CONSOLE_COMMAND(cmd, list_acl_rules, "ls", "acl", "rules");
    CONSOLE_COMMAND(cmd, list_subscription_of_user, "ls", "sub", "of",
                    CONSOLE_OPTION("filter_by", "user", "client_id"),
                    CONSOLE_VARIABLE("who"));
    CONSOLE_COMMAND(cmd, quit, "quit");
}

int main()
{
    /* ignore SIGPIPE signal */
    signal(SIGPIPE, SIG_IGN);
    tmq_console_cmd_t cmd;
    init_commands(&cmd);

    int conn = connect_to_broker();
    if(conn < 0)
    {
        printf("failed to connect to broker: %s\n", strerror(errno));
        return 0;
    }
    char* line = NULL;
    size_t len = 0;
    printf("tinymqtt> ");
    while(getline(&line, &len, stdin) != -1)
    {
        if(line)
        {
            // ignore LF
            line[strlen(line) - 1] = 0;
            if(strlen(line) > 0 && tmq_console_cmd_parse(&cmd, line, &conn) < 0)
                printf("syntax error in command: %s\n", line);
            free(line);
        }
        line = NULL;
        printf("tinymqtt> ");
    }
}