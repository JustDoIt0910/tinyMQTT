//
// Created by zr on 23-4-15.
//
#include "mqtt_tcp_conn.h"
#include "mqtt_buffer.h"
#include "tlog.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

static void destroy_cb_(void* arg)
{
    assert(arg != NULL);
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) arg;
    assert(conn->ref_cnt == 0);
    free(conn->read_event_handler);
    free(conn->read_event_handler);
    if(conn->write_event_handler)
        free(conn->write_event_handler);
    tmq_buffer_free(&conn->in_buffer);
    tmq_buffer_free(&conn->out_buffer);
    tmq_socket_close(conn->fd);
}

static void tmq_tcp_conn_close(tmq_tcp_conn_t* conn)
{
    tmq_event_loop_unregister(conn->loop, conn->read_event_handler);
    tmq_event_loop_unregister(conn->loop, conn->write_event_handler);
    tmq_event_loop_unregister(conn->loop, conn->error_close_handler);
    if(conn->close_cb)
        conn->close_cb(getRef(conn));
    releaseRef(conn);
}

static void read_cb_(tmq_socket_t fd, uint32_t event, const void* arg)
{
    if(!arg) return;
    tmq_tcp_conn_t* conn = getRef((tmq_tcp_conn_t*) arg);
    pthread_mutex_lock(&conn->lk);
    ssize_t n = tmq_buffer_read_fd(&conn->in_buffer, fd, 0);
    if(n == 0)
    {
        pthread_mutex_unlock(&conn->lk);
        tmq_tcp_conn_close(getRef(conn));
    }
    else if(n < 0)
    {
        pthread_mutex_unlock(&conn->lk);
        if(conn->error_cb)
            conn->error_cb(getRef(conn));
    }
    else
    {
        pthread_mutex_unlock(&conn->lk);
        if(conn->message_cb)
            conn->message_cb(getRef(conn), &conn->in_buffer);
    }
    releaseRef(conn);
}

static void write_cb_(tmq_socket_t fd, uint32_t event, const void* arg)
{

}

static void close_cb_(tmq_socket_t fd, uint32_t event, const void* arg)
{

}

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_event_loop_t* loop, tmq_socket_t fd,
                                 tcp_message_cb message_cb, tcp_close_cb close_cb, tcp_error_cb error_cb)
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
    conn->error_cb = error_cb;
    conn->destroy_cb = destroy_cb_;
    tmq_buffer_init(&conn->in_buffer);
    tmq_buffer_init(&conn->out_buffer);
    conn->loop = loop;
    conn->read_event_handler = tmq_event_handler_create(fd, EPOLLIN | EPOLLRDHUP, read_cb_, conn);
    if(!conn->read_event_handler)
        goto cleanup;
    conn->error_close_handler = tmq_event_handler_create(fd, EPOLLERR | EPOLLHUP, close_cb_, conn);
    if(!conn->error_close_handler)
    {
        free(conn->read_event_handler);
        goto cleanup;
    }

    tmq_event_loop_register(conn->loop, conn->read_event_handler);
    return getRef(conn);

    cleanup:
    pthread_mutex_destroy(&conn->lk);
    free(conn);
    return NULL;
}

int tmq_tcp_conn_id(tmq_tcp_conn_t* conn, char* buf, size_t buf_size)
{
    if(!conn) return -1;
    if(tmq_addr_to_string(&conn->peer_addr, buf, buf_size) < 0)
        return -1;
    return 0;
}