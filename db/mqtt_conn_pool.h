//
// Created by just do it on 2024/1/16.
//

#ifndef TINYMQTT_MQTT_CONN_POOL_H
#define TINYMQTT_MQTT_CONN_POOL_H
#include <mysql.h>
#include <pthread.h>
#include "base/mqtt_map.h"

typedef char* tmq_str_t;
typedef tmq_map(uintptr_t , int) mysql_conn_set;

typedef struct tmq_mysql_conn_pool_s
{
    tmq_str_t db_name;
    tmq_str_t host;
    tmq_str_t username;
    tmq_str_t password;
    uint16_t port;
    int pool_size;
    mysql_conn_set using_conns;
    mysql_conn_set idle_conns;
    pthread_mutex_t lk;
    pthread_cond_t cond;
} tmq_mysql_conn_pool_t;

void tmq_mysql_conn_pool_init(tmq_mysql_conn_pool_t* pool, const char* host, uint16_t port,
                              const char* username, const char* password, const char* db_name,
                              int pool_size);
MYSQL* tmq_mysql_conn_pool_pop(tmq_mysql_conn_pool_t* pool);
void tmq_mysql_conn_pool_push(tmq_mysql_conn_pool_t* pool, MYSQL* conn);
void tmq_mysql_conn_pool_destroy(tmq_mysql_conn_pool_t* pool);

#endif //TINYMQTT_MQTT_CONN_POOL_H
