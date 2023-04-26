//
// Created by zr on 23-4-15.
//
#include "mqtt_tcp_conn.h"
#include "mqtt_buffer.h"
#include "tlog.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void destroy_cb(void* conn)
{

}

static void read_cb(tmq_socket_t fd, uint32_t event, const void* arg)
{

}

static void write_cb(tmq_socket_t fd, uint32_t event, const void* arg)
{

}

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_event_loop_t* loop, tmq_socket_t fd,
                                 tcp_message_cb message_cb, tcp_close_cb close_cb)
{
    if(fd < 0) return NULL;
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) malloc(sizeof(tmq_tcp_conn_t));
    if(!conn) return NULL;
    bzero(conn, sizeof(tmq_tcp_conn_t));
    if(pthread_mutex_init(&conn->lk, NULL))
    {
        tlog_fatal("pthread_mutex_init() error %d: %s", errno, strerror(errno));
        tlog_exit();
        abort();
    }
    conn->fd = fd;
    tmq_socket_local_addr(conn->fd, &conn->local_addr);
    tmq_socket_peer_addr(conn->fd, &conn->peer_addr);
    conn->message_cb = message_cb;
    conn->close_cb = close_cb;
    conn->destroy_cb = destroy_cb;
    tmq_buffer_init(&conn->in_buffer);
    tmq_buffer_init(&conn->out_buffer);
    conn->loop = loop;
    conn->read_event_handler = tmq_event_handler_create(fd, EPOLLIN, read_cb, conn);
    if(!conn->read_event_handler)
    {
        pthread_mutex_destroy(&conn->lk);
        free(conn);
        return NULL;
    }
    tmq_event_loop_register(conn->loop, conn->read_event_handler);
    return getRef(conn);
}