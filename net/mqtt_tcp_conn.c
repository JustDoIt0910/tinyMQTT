//
// Created by zr on 23-4-15.
//
#include "mqtt_tcp_conn.h"
#include "mqtt/mqtt_broker.h"
#include "base/mqtt_util.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void read_cb_(tmq_socket_t fd, uint32_t event, const void* arg)
{
    if(!arg) return;
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) arg;
    ssize_t n = tmq_buffer_read_fd(&conn->in_buffer, fd, 0);
    if(n == 0)
        tmq_tcp_conn_close(get_ref(conn));
    else if(n < 0)
    {
        tlog_error("tmq_buffer_read_fd() error: n < 0");
        tmq_tcp_conn_close(get_ref(conn));
    }
    else
    {
        if(conn->codec)
            conn->codec->decode_tcp_message(conn->codec, conn, &conn->in_buffer);
    }
}

static void write_cb_(tmq_socket_t fd, uint32_t event, const void* arg)
{
    if(!arg) return;
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) arg;
    if(!conn->is_writing) return;
    ssize_t n = tmq_buffer_write_fd(&conn->out_buffer, fd);
    if(n >= 0)
    {
        if(conn->out_buffer.readable_bytes == 0)
        {
            tmq_handler_unregister(&conn->group->loop, conn->write_event_handler);
            conn->is_writing = 0;
        }
    }
    else
        tlog_error("tmq_buffer_write_fd() error");
}

static void close_cb_(tmq_socket_t fd, uint32_t event, const void* arg)
{
    if(!arg) return;
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) arg;
    if(event & EPOLLERR)
    {
        tlog_error("epoll error %d: %s", errno, strerror(errno));
        tmq_tcp_conn_close(get_ref(conn));
    }
    else if(event & EPOLLHUP && !(event & EPOLLIN))
        tmq_tcp_conn_close(get_ref(conn));
}

static void free_conn(tmq_tcp_conn_t* conn)
{
    free(conn->read_event_handler);
    free(conn->error_close_handler);
    if(conn->write_event_handler)
        free(conn->write_event_handler);

    tmq_buffer_free(&conn->in_buffer);
    tmq_buffer_free(&conn->out_buffer);

    tmq_tcp_conn_set_context(conn, NULL);
    tmq_socket_close(conn->fd);

    char conn_name[50];
    tmq_tcp_conn_id(conn, conn_name, sizeof(conn_name));
    free(conn);
    tlog_info("free connection [%s]", conn_name);
}

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_io_group_t* group, tmq_socket_t fd, tmq_codec_t* codec)
{
    if(fd < 0) return NULL;
    tmq_tcp_conn_t* conn = malloc(sizeof(tmq_tcp_conn_t));
    if(!conn)
        fatal_error("malloc() error: out of memory");
    bzero(conn, sizeof(tmq_tcp_conn_t));

    conn->fd = fd;
    conn->group = group;
    conn->codec = codec;
    conn->context = NULL;
    conn->is_writing = 0;
    tmq_socket_local_addr(conn->fd, &conn->local_addr);
    tmq_socket_peer_addr(conn->fd, &conn->peer_addr);

    tmq_buffer_init(&conn->in_buffer);
    tmq_buffer_init(&conn->out_buffer);

    conn->read_event_handler = tmq_event_handler_new(fd, EPOLLIN | EPOLLRDHUP, read_cb_, conn);
    conn->error_close_handler = tmq_event_handler_new(fd, EPOLLERR | EPOLLHUP, close_cb_, conn);
    tmq_handler_register(&conn->group->loop, conn->read_event_handler);
    tmq_handler_register(&conn->group->loop, conn->error_close_handler);
    return conn;
}

void tmq_tcp_conn_write(tmq_tcp_conn_t* conn, char* data, size_t size)
{
    ssize_t wrote = 0;
    if(!conn->is_writing)
        wrote = tmq_socket_write(conn->fd, data, size);
    int error = 0;
    if(wrote < 0)
    {
        wrote = 0;
        /* EWOULDBLOCK(EAGAIN) isn't error */
        if(errno != EWOULDBLOCK)
            error = 1;
    }
    size_t remain = size - wrote;
    if(!error && remain)
    {
        tmq_buffer_append(&conn->out_buffer, data + wrote, remain);
        if(!conn->is_writing)
        {
            if(!conn->write_event_handler)
                conn->write_event_handler = tmq_event_handler_new(conn->fd, EPOLLOUT, write_cb_, conn);
            tmq_handler_register(&conn->group->loop, conn->write_event_handler);
            conn->is_writing = 1;
        }
    }
}

void tmq_tcp_conn_close(tmq_tcp_conn_t* conn)
{
    tmq_handler_unregister(&conn->group->loop, conn->read_event_handler);
    tmq_handler_unregister(&conn->group->loop, conn->error_close_handler);

    if(tmq_handler_is_registered(&conn->group->loop, conn->write_event_handler))
        tmq_handler_unregister(&conn->group->loop, conn->write_event_handler);

    if(conn->close_cb)
        conn->close_cb(get_ref(conn), conn->close_cb_arg);

    release_ref(conn);
}

int tmq_tcp_conn_id(tmq_tcp_conn_t* conn, char* buf, size_t buf_size)
{
    if(!conn) return -1;
    int ret = tmq_addr_to_string(&conn->peer_addr, buf, buf_size);
    return ret;
}

void tmq_tcp_conn_set_context(tmq_tcp_conn_t* conn, void* ctx)
{
    if(!conn) return;
    if(conn->context)
        free(conn->context);
    conn->context = ctx;
}

tmq_tcp_conn_t* get_ref(tmq_tcp_conn_t* conn)
{
    incrementAndGet(conn->ref_cnt, 1);
    return conn;
}

void release_ref(tmq_tcp_conn_t* conn)
{
    int n = decrementAndGet(conn->ref_cnt, 1);
    if(!n) free_conn(conn);
}