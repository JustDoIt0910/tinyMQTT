//
// Created by just do it on 2024/1/16.
//
#include "db/mqtt_conn_pool.h"
#include "db/mqtt_db.h"


int main()
{
    tmq_mysql_conn_pool_t pool;
    tmq_mysql_conn_pool_init(&pool, "localhost", 3306, "root", "20010910cheng", "tinymqtt_db", 5);
    MYSQL* conn;
    conn = tmq_mysql_conn_pool_pop(&pool);
    load_acl_from_mysql(conn, NULL);
}