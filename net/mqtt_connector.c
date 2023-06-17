//
// Created by zr on 23-6-17.
//
#include "mqtt_connector.h"
#include "tlog.h"
#include <errno.h>
#include <string.h>

static void reconnect(void* arg) { tmq_connector_connect((tmq_connector_t*) arg);}
static void retry(tmq_connector_t* connector)
{
    if(connector->retry_times >= connector->max_retry)
        return;
    tlog_info("reconnecting to server...");
    tmq_timer_t* timer = tmq_timer_new(SEC_MS(connector->retry_interval), 0, reconnect, connector);
    tmq_event_loop_add_timer(connector->loop, timer);
    connector->retry_interval *= 2;
    connector->retry_times++;
}

static void handle_write(tmq_socket_t sock, uint32_t events, void* arg)
{
    tmq_connector_t* connector = arg;
    tmq_handler_unregister(connector->loop, connector->write_handler);
    tmq_handler_unregister(connector->loop, connector->error_handler);

    /* check if connect successfully */
    int err = tmq_socket_get_error(sock);
    if(err)
    {
        tlog_error("connect falied: %s", strerror(err));
        tmq_socket_close(sock);
        retry(connector);
    }
    else connector->on_tcp_connect(sock);
}

static void handle_error(tmq_socket_t sock, uint32_t events, void* arg)
{
    tmq_connector_t* connector = arg;
    tmq_handler_unregister(connector->loop, connector->write_handler);
    tmq_handler_unregister(connector->loop, connector->error_handler);

    tlog_error("connect error: %s", strerror(errno));
    tmq_socket_close(sock);
    retry(connector);
}

static void continue_connect(tmq_connector_t* connector, tmq_socket_t sock)
{
    connector->write_handler = tmq_event_handler_new(sock, EPOLLOUT, handle_write, connector);
    connector->error_handler = tmq_event_handler_new(sock, EPOLLERR, handle_error, connector);
    tmq_handler_register(connector->loop, connector->write_handler);
    tmq_handler_register(connector->loop, connector->error_handler);
}

void tmq_connector_init(tmq_connector_t* connector, tmq_event_loop_t* loop, const char* server_ip, uint16_t server_port,
                        tcp_connected_cb on_connect, int max_retry)
{
    connector->loop = loop;
    connector->on_tcp_connect = on_connect;
    connector->server_addr = tmq_addr_from_ip_port(server_ip, server_port);
    connector->max_retry = max_retry;
    connector->retry_interval = INITIAL_RETRY_INTERVAL;
    connector->retry_times = 0;
}

void tmq_connector_connect(tmq_connector_t* connector)
{
    if(!connector || !connector->loop)
    {
        tlog_error("using connector without initialization");
        return;
    }
    tmq_socket_t sock = tmq_tcp_socket();
    int ret = tmq_socket_connect(sock, connector->server_addr);
    int err = (ret == 0) ? 0 : errno;
    switch (err)
    {
        case 0:
        case EINPROGRESS:
        case EISCONN:
        case EINTR:
            continue_connect(connector, sock);
            break;
        case EAGAIN:
        case ECONNREFUSED:
        case ENETUNREACH:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
            tmq_socket_close(sock);
            retry(connector);
            break;
        default:
            tlog_error("tmq_connector_connect() error %d: %s",err, strerror(err));
            break;
    }
}