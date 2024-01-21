//
// Created by zr on 23-4-5.
//

#ifndef TINYMQTT_MQTT_SOCKET_H
#define TINYMQTT_MQTT_SOCKET_H
#include <netinet/in.h>

typedef int tmq_socket_t;
typedef struct sockaddr_in tmq_socket_addr_t;
#define tmq_addr_is_valid(addr) !((addr).sin_addr.s_addr == 0 && (addr).sin_port == 0)

tmq_socket_t tmq_tcp_socket();
tmq_socket_t tmq_unix_socket(int nonblock);
int tmq_socket_nonblocking(tmq_socket_t fd);
void tmq_socket_close(tmq_socket_t fd);
void tmq_socket_shutdown(tmq_socket_t fd);
int tmq_socket_reuse_addr(tmq_socket_t fd, int enable);
int tmq_socket_reuse_port(tmq_socket_t fd, int enable);
int tmq_socket_keepalive(tmq_socket_t fd, int enable);
int tmq_socket_tcp_no_delay(tmq_socket_t fd, int enable);
int tmq_socket_local_addr(tmq_socket_t fd, tmq_socket_addr_t* addr);
int tmq_socket_peer_addr(tmq_socket_t fd, tmq_socket_addr_t* addr);
void tmq_socket_bind(tmq_socket_t fd, const char* ip, int port);
void tmq_unix_socket_bind(tmq_socket_t fd, const char* path);
void tmq_socket_listen(tmq_socket_t fd);
tmq_socket_t tmq_socket_accept(tmq_socket_t fd, tmq_socket_addr_t* clientAddr);
int tmq_socket_connect(tmq_socket_t fd, tmq_socket_addr_t addr);
int tmq_unix_socket_connect(tmq_socket_t fd, const char* path);
ssize_t tmq_socket_read(tmq_socket_t fd, char* buf, size_t len);
ssize_t tmq_socket_write(tmq_socket_t fd, const char* buf, size_t len);
int tmq_socket_get_error(tmq_socket_t fd);

tmq_socket_addr_t tmq_addr_from_ip_port(const char* ip, uint16_t port);
tmq_socket_addr_t tmq_addr_from_port(uint16_t port, int loop_back);
int tmq_addr_to_string(tmq_socket_addr_t* addr, char* buf, size_t bufLen);

#endif //TINYMQTT_MQTT_SOCKET_H
