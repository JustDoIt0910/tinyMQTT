//
// Created by zr on 23-4-15.
//
#include "mqtt_tcp_conn.h"
#include "mqtt/mqtt_broker.h"
#include "base/mqtt_util.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

extern SSL_CTX* g_ssl_ctx;

static void conn_close_(tmq_tcp_conn_t* conn);
static void ssl_handshake(tmq_tcp_conn_t* conn);

static void read_cb_(tmq_socket_t fd, uint32_t event, void* arg)
{
    if(!arg) return;
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) arg;
    if(conn->state == CONNECTED)
    {
        if(conn->ssl)
        {
            char buf[65536];
            int wd = SSL_read(conn->ssl, buf, sizeof(buf));
            if(wd <= 0)
            {
                int err = SSL_get_error(conn->ssl, wd);
                if(wd == 0 && err == SSL_ERROR_ZERO_RETURN)
                    conn_close_(conn);
                else if(err != SSL_ERROR_WANT_READ)
                {
                    tlog_error("SSL_read() return: %d error: %d errno: %d %s", wd, err, errno, strerror(errno));
                    conn_close_(conn);
                }
                return;
            }
            tmq_buffer_append(&conn->in_buffer, buf, wd);
            if(conn->codec)
                conn->codec->decode_tcp_message(conn->codec, conn, &conn->in_buffer);
            return;
        }
        ssize_t n = tmq_buffer_read_fd(&conn->in_buffer, fd, 0);
        if(n == 0)
            conn_close_(conn);
        else if(n < 0)
        {
            if(errno == EINTR)
                return;
            tlog_error("tmq_buffer_read_fd() error: n < 0");
            conn_close_(conn);
        }
        else
        {
            if(conn->codec)
                conn->codec->decode_tcp_message(conn->codec, conn, &conn->in_buffer);
        }
    }
    else if(conn->state == SSL_HANDSHAKING)
        ssl_handshake(conn);
}

static void write_cb_(tmq_socket_t fd, uint32_t event, void* arg)
{
    if(!arg) return;
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) arg;

    if(conn->state == SSL_HANDSHAKING)
    {
        ssl_handshake(conn);
        return;
    }

    if(!conn->is_writing || conn->state == DISCONNECTED)
        return;
    ssize_t n;
    if(conn->ssl)
    {
        char buf[INT_MAX];
        size_t size = min(conn->out_buffer.readable_bytes, INT_MAX);
        tmq_buffer_peek(&conn->out_buffer, buf, size);
        n = SSL_write(conn->ssl, buf, (int)size);
    }
    else
        n = tmq_buffer_write_fd(&conn->out_buffer, fd);
    if(n > 0)
    {
        if(conn->ssl)
            tmq_buffer_remove(&conn->out_buffer, n);

        if(conn->out_buffer.readable_bytes == 0)
        {
            tmq_handler_unregister(conn->loop, conn->write_event_handler);
            conn->is_writing = 0;
            if(conn->on_write_complete)
                conn->on_write_complete(conn->cb_arg);
            if(conn->state == DISCONNECTING)
                tmq_tcp_conn_shutdown(conn);
        }
    }
    else
    {
        if(conn->ssl)
        {
            int err = SSL_get_error(conn->ssl, (int)n);
            if(err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
            {
                tlog_error("SSL_write() return: %d error: %d errno: %d %s", (int)n, err, errno, strerror(errno));
                conn_close_(conn);
            }
            return;
        }
        if(n < 0)
        {
            if(errno == EINTR)
                return;
            tlog_info("tmq_buffer_write_fd() error: %s", strerror(errno));
        }
    	conn->is_writing = 0;
        conn_close_(conn);
    }
}

static void ssl_handshake(tmq_tcp_conn_t* conn)
{
    int r = SSL_do_handshake(conn->ssl);
    if(r == 1)
    {
        conn->state = CONNECTED;
        tlog_info("handshake success");
        if(conn->write_event_handler && tmq_handler_is_registered(conn->loop, conn->write_event_handler))
            tmq_handler_unregister(conn->loop, conn->write_event_handler);
        return;
    }
    int err = SSL_get_error(conn->ssl, r);
    if(err == SSL_ERROR_WANT_WRITE)
    {
        if(!conn->write_event_handler)
            conn->write_event_handler = tmq_event_handler_new(conn->fd, EPOLLOUT, write_cb_, conn, 1);
        if(!tmq_handler_is_registered(conn->loop, conn->write_event_handler))
            tmq_handler_register(conn->loop, conn->write_event_handler);
    }
    else if(err == SSL_ERROR_WANT_READ) { /*noting to do*/ }
    else
    {
        tlog_error("SSL_do_handshake() error");
        conn_close_(conn);
    }
}

static void close_cb_(tmq_socket_t fd, uint32_t event, void* arg)
{
    if(!arg) return;
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) arg;
    if(event & EPOLLERR)
    {
        tlog_error("epoll error %d: %s", errno, strerror(errno));
        conn_close_(conn);
    }
    else if(event & EPOLLHUP && !(event & EPOLLIN))
        conn_close_(conn);
}

static void conn_close_(tmq_tcp_conn_t* conn)
{
    if(conn->state == DISCONNECTED) return;
    tmq_handler_unregister(conn->loop, conn->read_event_handler);
    tmq_handler_unregister(conn->loop, conn->error_close_handler);

    if(tmq_handler_is_registered(conn->loop, conn->write_event_handler))
        tmq_handler_unregister(conn->loop, conn->write_event_handler);

    if(conn->on_close)
        conn->on_close(conn, conn->cb_arg);

    conn->state = DISCONNECTED;
}

static void conn_cleanup_(tmq_ref_counted_t* obj)
{
    tmq_tcp_conn_t* conn = (tmq_tcp_conn_t*) obj;
    free(conn->read_event_handler);
    free(conn->error_close_handler);
    if(conn->write_event_handler)
        free(conn->write_event_handler);

    tmq_buffer_free(&conn->in_buffer);
    tmq_buffer_free(&conn->out_buffer);

    tmq_tcp_conn_set_context(conn, NULL, NULL);
    tmq_socket_close(conn->fd);

    if(conn->ssl)
    {
        SSL_shutdown(conn->ssl);
        SSL_free(conn->ssl);
    }
}

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_event_loop_t* loop, tmq_io_context_t* io_context,
                                 tmq_socket_t fd, int is_ssl, tmq_codec_t* codec)
{
    if(fd < 0) return NULL;
    tmq_tcp_conn_t* conn = malloc(sizeof(tmq_tcp_conn_t));
    if(!conn)
        fatal_error("malloc() error: out of memory");
    bzero(conn, sizeof(tmq_tcp_conn_t));

    conn->cleaner = conn_cleanup_;
    conn->fd = fd;
    conn->loop = loop;
    conn->io_context = io_context;
    conn->codec = codec;
    conn->context = NULL;
    conn->is_writing = 0;
    conn->state = is_ssl ? SSL_HANDSHAKING: CONNECTED;
    tmq_socket_local_addr(conn->fd, &conn->local_addr);
    tmq_socket_peer_addr(conn->fd, &conn->peer_addr);

    tmq_buffer_init(&conn->in_buffer);
    tmq_buffer_init(&conn->out_buffer);

    conn->read_event_handler = tmq_event_handler_new(fd, EPOLLIN | EPOLLRDHUP, read_cb_, conn, 1);
    conn->error_close_handler =  tmq_event_handler_new(fd, EPOLLERR | EPOLLHUP, close_cb_, conn, 1);
    tmq_handler_register(conn->loop, conn->read_event_handler);
    tmq_handler_register(conn->loop, conn->error_close_handler);

    if(is_ssl)
    {
        conn->ssl = SSL_new(g_ssl_ctx);
        SSL_set_fd(conn->ssl, fd);
        SSL_set_accept_state(conn->ssl);
    }

    return conn;
}

void tmq_tcp_conn_write(tmq_tcp_conn_t* conn, char* data, size_t size)
{
    if(conn->state == DISCONNECTED) return;
    ssize_t wrote = 0;
    if(!conn->is_writing)
    {
        if(!conn->ssl)
            wrote = tmq_socket_write(conn->fd, data, size);
        else
        {
            size = min(size, INT_MAX);
            wrote = SSL_write(conn->ssl, data, (int)size);
        }
    }
    if(wrote < 0)
    {
        if(conn->ssl)
        {
            int err = SSL_get_error(conn->ssl, (int)wrote);
            if(err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
            {
                tlog_error("SSL_write() return: %d error: %d errno: %d %s", (int)wrote, err, errno, strerror(errno));
                conn_close_(conn);
                return;
            }
        }
        else
        {
            /* EWOULDBLOCK(EAGAIN) isn't an error */
            if(errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR)
            {
                tlog_info("write error: %s", strerror(errno));
                conn_close_(conn);
                return;
            }
        }
        wrote = 0;
    }
    size_t remain = size - wrote;
    if(!remain && conn->on_write_complete)
        conn->on_write_complete(conn->cb_arg);
    if(remain)
    {
        tmq_buffer_append(&conn->out_buffer, data + wrote, remain);
        if(!conn->is_writing)
        {
            if(!conn->write_event_handler)
                conn->write_event_handler = tmq_event_handler_new(conn->fd, EPOLLOUT, write_cb_, conn, 1);
            tmq_handler_register(conn->loop, conn->write_event_handler);
            conn->is_writing = 1;
        }
    }
}

void tmq_tcp_conn_shutdown(tmq_tcp_conn_t* conn)
{
    if(conn->state == DISCONNECTED)
        return;
    if(conn->is_writing)
    {
        conn->state = DISCONNECTING;
        return;
    }
    tmq_socket_shutdown(conn->fd);
}

void tmq_tcp_conn_force_close(tmq_tcp_conn_t* conn) {conn_close_(conn);}

int tmq_tcp_conn_id(tmq_tcp_conn_t* conn, char* buf, size_t buf_size)
{
    if(!conn) return -1;
    int ret = tmq_addr_to_string(&conn->peer_addr, buf, buf_size);
    return ret;
}

sa_family_t tmq_tcp_conn_family(tmq_tcp_conn_t* conn) { return conn->local_addr.sin_family; }

void tmq_tcp_conn_set_codec(tmq_tcp_conn_t* conn, tmq_codec_t* codec) { conn->codec = codec; }

void tmq_tcp_conn_set_context(tmq_tcp_conn_t* conn, void* ctx, context_cleanup_cb clean_up)
{
    if(!conn) return;
    if(conn->context)
    {
        if(conn->ctx_clean_up)
            conn->ctx_clean_up(conn->context);
        free(conn->context);
    }
    conn->context = ctx;
    conn->ctx_clean_up = clean_up;
}

void tmq_tcp_conn_free(tmq_tcp_conn_t* conn)
{
    conn_cleanup_((tmq_ref_counted_t*) conn);
    free(conn);
}