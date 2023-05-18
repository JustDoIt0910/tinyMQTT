//
// Created by zr on 23-4-20.
//
#include "mqtt_session.h"
#include "mqtt_tcp_conn.h"

typedef struct tmq_session_s
{
    tmq_tcp_conn_t* tcp_conn;
} tmq_session_t;