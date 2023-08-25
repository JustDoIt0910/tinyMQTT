//
// Created by zr on 23-4-5.
//
#include "mqtt_socket.h"
#include "tlog.h"
#include "mqtt_util.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

tmq_socket_t tmq_tcp_socket()
{
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if(fd < 0)
        fatal_error("socket() error %d: %s", errno, strerror(errno));
    return fd;
}

int tmq_socket_nonblocking(tmq_socket_t fd)
{
    int ops = fcntl(fd, F_GETFL);
    if(ops < 0)
        fatal_error("fcntl() error %d: %s", errno, strerror(errno));
    ops |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, ops) < 0)
    {
        fatal_error("fcntl() error %d: %s", errno, strerror(errno));
        tlog_exit();
        abort();
    }
    return 0;
}

void tmq_socket_close(tmq_socket_t fd) { close(fd);}

void tmq_socket_shutdown(tmq_socket_t fd)
{
    shutdown(fd, SHUT_WR);
}

int tmq_socket_reuse_addr(tmq_socket_t fd, int enable)
{
    assert(enable == 0 || enable == 1);
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, (socklen_t) sizeof(enable)) < 0)
        fatal_error("setsockopt() error %d: %s", errno, strerror(errno));
    return 0;
}

int tmq_socket_reuse_port(tmq_socket_t fd, int enable)
{
    assert(enable == 0 || enable == 1);
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, (socklen_t) sizeof(enable)) < 0)
        fatal_error("setsockopt() error %d: %s", errno, strerror(errno));
    return 0;
}

int tmq_socket_keepalive(tmq_socket_t fd, int enable)
{
    assert(enable == 0 || enable == 1);
    if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, (socklen_t) sizeof(enable)) < 0)
        fatal_error("setsockopt() error %d: %s", errno, strerror(errno));
    return 0;
}

int tmq_socket_tcp_no_delay(tmq_socket_t fd, int enable)
{
    assert(enable == 0 || enable == 1);
    if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, (socklen_t) sizeof(enable)) < 0)
        fatal_error("setsockopt() error %d: %s", errno, strerror(errno));
    return 0;
}

int tmq_socket_local_addr(tmq_socket_t fd, tmq_socket_addr_t* addr)
{
    socklen_t len = sizeof(tmq_socket_addr_t);
    if(getsockname(fd, (struct sockaddr*)(addr), &len) < 0)
    {
        tlog_error("getsockname() error %d: %s", errno, strerror(errno));
        return -1;
    }
    return 0;
}

int tmq_socket_peer_addr(tmq_socket_t fd, tmq_socket_addr_t* addr)
{
    socklen_t len = sizeof(tmq_socket_addr_t);
    if(getpeername(fd, (struct sockaddr*)(addr), &len) < 0)
    {
        tlog_error("getpeername() error %d: %s", errno, strerror(errno));
        return -1;
    }
    return 0;
}

void tmq_socket_bind(tmq_socket_t fd, const char* ip, int port)
{
    tmq_socket_addr_t addr;
    if(ip)
        addr = tmq_addr_from_ip_port(ip, port);
    else
        addr = tmq_addr_from_port(port, 0);
    if(bind(fd, (struct sockaddr*)(&addr), sizeof(addr)) < 0)
        fatal_error("bind() error %d: %s", errno, strerror(errno));
}

void tmq_socket_listen(tmq_socket_t fd)
{
    if(listen(fd, 32768) < 0)
        fatal_error("listen() error %d: %s", errno, strerror(errno));
}

tmq_socket_t tmq_socket_accept(tmq_socket_t fd, tmq_socket_addr_t* clientAddr)
{
    socklen_t len = sizeof(tmq_socket_addr_t);
    int clientFd = accept4(fd, (struct sockaddr*)clientAddr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(clientFd < 0)
    {
        int savedErrno = errno;
        switch (savedErrno)
        {
            case EAGAIN:
            case ECONNABORTED:
            case EINTR:
            case EPROTO:
            case EPERM:
            case EMFILE:
                errno = savedErrno;
                break;
            default:
                fatal_error("accept4() error %d: %s", errno, strerror(errno));
        }
    }
    return clientFd;
}

int tmq_socket_connect(tmq_socket_t fd, tmq_socket_addr_t addr)
{
    socklen_t len = sizeof(tmq_socket_addr_t);
    return connect(fd, (struct sockaddr*)&addr, len);
}

ssize_t tmq_socket_read(tmq_socket_t fd, char* buf, size_t len) { return read(fd, buf, len);}

ssize_t tmq_socket_write(tmq_socket_t fd, const char* buf, size_t len) { return write(fd, buf, len);}

int tmq_socket_get_error(tmq_socket_t fd)
{
    int opt;
    socklen_t len = sizeof(opt);
    if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &opt, &len) < 0)
        return errno;
    return opt;
}

tmq_socket_addr_t tmq_addr_from_ip_port(const char* ip, uint16_t port)
{
    tmq_socket_addr_t addr;
    bzero(&addr, sizeof(addr));
    if(!ip) return addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    addr.sin_port = htobe16(port);
    return addr;
}

tmq_socket_addr_t tmq_addr_from_port(uint16_t port, int loopBack)
{
    tmq_socket_addr_t addr;
    bzero(&addr, sizeof(addr));
    addr.sin_port = htobe16(port);
    in_addr_t address = loopBack ? INADDR_LOOPBACK : INADDR_ANY;
    addr.sin_addr.s_addr = htobe32(address);
    return addr;
}

int tmq_addr_to_string(tmq_socket_addr_t* addr, char* buf, size_t bufLen)
{
    if(!buf)
    {
        tlog_error("tmq_addr_to_string() error: buf can't be NULL");
        return -1;
    }
    //xxx.xxx.xxx.xxx:ppppp
    if(bufLen < INET_ADDRSTRLEN + 1 + 5)
    {
        tlog_error("tmq_addr_to_string() error: buf size must greater than 22");
        return -1;
    }
    bzero(buf, bufLen);
    inet_ntop(AF_INET, &addr->sin_addr, buf, bufLen);
    size_t addrLen = strlen(buf);
    snprintf(buf + addrLen, bufLen - addrLen, ":%u", be16toh(addr->sin_port));
    return 0;
}
