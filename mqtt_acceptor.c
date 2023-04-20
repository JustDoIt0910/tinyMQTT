//
// Created by zr on 23-4-18.
//
#include "mqtt_acceptor.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static void acceptor_cb(tmq_socket_t fd, uint32_t event, const void* arg)
{
    tmq_acceptor_t* acceptor = (tmq_acceptor_t*) arg;
    tmq_socket_addr_t peer_addr;
    tmq_socket_t conn = tmq_socket_accept(fd, &peer_addr);
    if(conn < 0 && errno == EMFILE)
    {
        close(acceptor->idle_socket);
        acceptor->idle_socket = accept(fd, NULL, NULL);
        close(acceptor->idle_socket);
        acceptor->idle_socket = open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
    else if(acceptor->connection_cb)
        acceptor->connection_cb(conn, &peer_addr, acceptor->arg);
}

void tmq_acceptor_init(tmq_acceptor_t* acceptor, tmq_event_loop_t* loop, const void* arg, uint16_t port)
{
    if(!acceptor) return;
    acceptor->listening = 0;
    acceptor->lis_socket = tmq_tcp_socket();
    tmq_socket_reuse_addr(acceptor->lis_socket, 1);
    tmq_socket_bind(acceptor->lis_socket, NULL, port);
    acceptor->idle_socket = open("/dev/null", O_RDONLY | O_CLOEXEC);
    acceptor->arg = arg;
    tmq_event_handler_t* handler = tmq_event_handler_create(acceptor->lis_socket, EPOLLIN,
                                                            acceptor_cb, acceptor);
    tmq_event_loop_register(loop, handler);
}

void tmq_acceptor_listen(tmq_acceptor_t* acceptor)
{
    if(atomicExchange(acceptor->listening, 1))
        return;
    tmq_socket_listen(acceptor->lis_socket);
}

void tmq_acceptor_set_cb(tmq_acceptor_t* acceptor, tmq_new_connection_cb cb)
{
    acceptor->connection_cb = cb;
}