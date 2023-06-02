//
// Created by zr on 23-4-20.
//
#include "mqtt_session.h"
#include "net/mqtt_tcp_conn.h"
#include "base/mqtt_util.h"
#include <stdlib.h>
#include <string.h>

tmq_session_t* tmq_session_new(tmq_tcp_conn_t* conn, int clean_session)
{
    tmq_session_t* session = malloc(sizeof(tmq_session_t));
    if(!session) fatal_error("malloc() error: out of memoty");
    bzero(session, sizeof(tmq_session_t));
    session->state = OPEN;
    session->clean_session = clean_session;
    session->conn = get_ref(conn);
    return session;
}