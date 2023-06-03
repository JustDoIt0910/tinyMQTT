//
// Created by zr on 23-5-26.
//
#include "base/mqtt_cmd.h"
#include "base/mqtt_config.h"
#include "base/mqtt_util.h"
#include <termios.h>
#include <string.h>

void password_mode(int on)
{
    struct termios new_setting, init_setting;

    tcgetattr(0,&init_setting);
    new_setting = init_setting;
    if(on) new_setting.c_lflag &= ~ECHO;
    else new_setting.c_lflag |= ECHO;
    tcsetattr(0, TCSANOW, &new_setting);
}

int get_password(char* user, char pwd[])
{
    printf("please enter password for user \"%s\":\n", user);
    password_mode(1);
    char pwd1[50] = {0}; char pwd2[50] = {0};
    fgets(pwd1, sizeof(pwd1), stdin);
    pwd1[strlen(pwd1) - 1] = 0;
    printf("please confirm password for user \"%s\":\n", user);
    fgets(pwd2, sizeof(pwd2), stdin);
    pwd2[strlen(pwd2) - 1] = 0;
    password_mode(0);
    if(strcmp(pwd1, pwd2) != 0)
    {
        printf("inconsistent passwords.\n");
        return -1;
    }
    strcpy(pwd, pwd1);
    return 0;
}

int main(int argc, char* argv[])
{
    tmq_cmd_t cmd;
    tmq_cmd_init(&cmd);
    tmq_cmd_add_string(&cmd, "config", "c", "config file path", 1, "");
    tmq_cmd_add_string(&cmd, "add", "A", "create new user", 0, "");
    tmq_cmd_add_string(&cmd, "mod", "M", "modify user password", 0, "");
    tmq_cmd_add_string(&cmd, "del", "D", "delete user", 0, "");

    if(tmq_cmd_parse(&cmd, argc, argv) != -1)
    {
        tmq_config_t pwd_cfg;
        tmq_str_t pwd_path = tmq_cmd_get_string(&cmd, "config");
        if(tmq_config_init(&pwd_cfg, pwd_path, ":") < 0)
        {
            tmq_str_free(pwd_path);
            tmq_cmd_destroy(&cmd);
            return 0;
        }
        if(tmq_cmd_exist(&cmd, "add"))
        {
            tmq_str_t user = tmq_cmd_get_string(&cmd, "add");
            if(tmq_config_exist(&pwd_cfg, user))
            {
                printf("username exist.\n");
                tmq_str_free(user);
                tmq_cmd_destroy(&cmd);
                tmq_config_destroy(&pwd_cfg);
                return 0;
            }
            char pwd[50] = {0};
            if(get_password(user, pwd) < 0)
            {
                tmq_str_free(user);
                tmq_cmd_destroy(&cmd);
                tmq_config_destroy(&pwd_cfg);
                return 0;
            }
            char* pwd_encoded = password_encode(pwd);
            tmq_config_add(&pwd_cfg, user, pwd_encoded);
            tmq_str_free(user);
            free(pwd_encoded);
            printf("add user ok.\n");
        }
        else if(tmq_cmd_exist(&cmd, "mod"))
        {
            tmq_str_t user = tmq_cmd_get_string(&cmd, "mod");
            if(!tmq_config_exist(&pwd_cfg, user))
            {
                printf("user doesn't exist.\n");
                tmq_str_free(user);
                tmq_cmd_destroy(&cmd);
                tmq_config_destroy(&pwd_cfg);
                return 0;
            }
            char pwd[50] = {0};
            if(get_password(user, pwd) < 0)
            {
                tmq_str_free(user);
                tmq_cmd_destroy(&cmd);
                tmq_config_destroy(&pwd_cfg);
                return 0;
            }
            char* pwd_encoded = password_encode(pwd);
            tmq_config_mod(&pwd_cfg, user, pwd_encoded);
            tmq_str_free(user);
            free(pwd_encoded);
            printf("modify password ok.\n");
        }
        else if(tmq_cmd_exist(&cmd, "del"))
        {
            tmq_str_t user = tmq_cmd_get_string(&cmd, "del");
            if(!tmq_config_exist(&pwd_cfg, user))
            {
                printf("user doesn't exist.\n");
                tmq_str_free(user);
                tmq_cmd_destroy(&cmd);
                tmq_config_destroy(&pwd_cfg);
                return 0;
            }
            tmq_config_del(&pwd_cfg, user);
            tmq_str_free(user);
            printf("delete user ok.\n");
        }
        tmq_config_destroy(&pwd_cfg);
    }
    tmq_cmd_destroy(&cmd);
}