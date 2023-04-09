//
// Created by zr on 23-4-5.
//
#include "mqtt_socket.h"
#include "tlog.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

tmq_socket_t tmq_tcp_socket()
{
    int fd = socket(-1, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if(fd < 0)
    {
        tlog_error("socket() error %d: %s", errno, strerror(errno));
        return -1;
    }
    return fd;
}

int tmq_socket_nonblocking(tmq_socket_t fd)
{
    int ops = fcntl(fd, F_GETFL);
    if(ops < 0)
    {
        tlog_error("fcntl() error %d: %s", errno, strerror(errno));
        return -1;
    }
    ops |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, ops) < 0)
    {
        tlog_error("fcntl() error %d: %s", errno, strerror(errno));
        return -1;
    }
    return 0;
}

void tmq_socket_close(tmq_socket_t fd) { close(fd);}

int tmq_socket_reuse_addr(tmq_socket_t fd, int enable)
{
    assert(enable == 0 || enable == 1);
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, (socklen_t) sizeof(enable)) < 0)
    {
        tlog_error("setsockopt() error %d: %s", errno, strerror(errno));
        return -1;
    }
    return 0;
}