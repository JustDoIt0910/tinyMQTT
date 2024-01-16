//
// Created by just do it on 2024/1/16.
//
#include <assert.h>
#include "mqtt_conn_pool.h"
#include "base/mqtt_str.h"
#include "base/mqtt_util.h"

void tmq_mysql_conn_pool_init(tmq_mysql_conn_pool_t* pool, const char* host, uint16_t port,
                              const char* username, const char* password, const char* db_name,
                              int pool_size)
{
    if (mysql_library_init(0, NULL, NULL))
        fatal_error("could not initialize MySQL client library");
    pool->host = tmq_str_new(host);
    pool->port = port;
    pool->username = tmq_str_new(username);
    pool->password = tmq_str_new(password);
    pool->db_name = tmq_str_new(db_name);
    pool->pool_size = pool_size;
    tmq_map_64_init(&pool->using_conns, int, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_map_64_init(&pool->idle_conns, int, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    pthread_mutex_init(&pool->lk, NULL);
    pthread_cond_init(&pool->cond, NULL);
}

MYSQL* tmq_mysql_conn_pool_pop(tmq_mysql_conn_pool_t* pool)
{
    pthread_mutex_lock(&pool->lk);
    while(tmq_map_size(pool->using_conns) == pool->pool_size)
        pthread_cond_wait(&pool->cond, &pool->lk);
    assert(tmq_map_size(pool->using_conns) + tmq_map_size(pool->idle_conns) < pool->pool_size ||
    tmq_map_size(pool->idle_conns) > 0);
    if(tmq_map_size(pool->idle_conns) > 0)
    {
        tmq_map_iter_t it = tmq_map_iter(pool->idle_conns);
        MYSQL* conn = (MYSQL*)(*(uintptr_t*)it.first);
        tmq_map_put(pool->using_conns, (uintptr_t)conn, 1);
        pthread_mutex_unlock(&pool->lk);
        return conn;
    }
    MYSQL* conn = malloc(sizeof(MYSQL));
    if(!conn)
        fatal_error("malloc() error: out of memory");
    mysql_init(conn);
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    if(!mysql_real_connect(conn, pool->host, pool->username, pool->password,
                           pool->db_name, pool->port, NULL, 0))
    {
        tlog_error("mysql_real_connect() error");
        free(conn);
        return NULL;
    }
    tmq_map_put(pool->using_conns, (uintptr_t)conn, 1);
    pthread_mutex_unlock(&pool->lk);
    return conn;
}

void tmq_mysql_conn_pool_push(tmq_mysql_conn_pool_t* pool, MYSQL* conn)
{
    pthread_mutex_lock(&pool->lk);
    int* present = tmq_map_get(pool->using_conns, (uintptr_t)conn);
    if(!present)
    {
        pthread_mutex_unlock(&pool->lk);
        return;
    }
    tmq_map_erase(pool->using_conns, (uintptr_t)conn);
    tmq_map_put(pool->idle_conns, (uintptr_t)conn, 1);
    pthread_mutex_unlock(&pool->lk);
    pthread_cond_broadcast(&pool->cond);
}

void tmq_mysql_conn_pool_destroy(tmq_mysql_conn_pool_t* pool)
{
    assert(tmq_map_size(pool->using_conns) == 0);
    tmq_map_iter_t it = tmq_map_iter(pool->idle_conns);
    for(; tmq_map_has_next(it); tmq_map_next(pool->idle_conns, it))
    {
        MYSQL* conn = (MYSQL*)(*(uintptr_t*)it.first);
        mysql_close(conn);
        free(conn);
    }
    tmq_map_free(pool->using_conns);
    tmq_map_free(pool->idle_conns);
    tmq_str_free(pool->host);
    tmq_str_free(pool->username);
    tmq_str_free(pool->password);
    tmq_str_free(pool->db_name);
    pthread_mutex_destroy(&pool->lk);
    pthread_cond_destroy(&pool->cond);
    mysql_library_end();
}