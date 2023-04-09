//
// Created by zr on 23-4-5.
//

#ifndef TINYMQTT_MQTT_SOCKET_H
#define TINYMQTT_MQTT_SOCKET_H

typedef int tmq_socket_t;

tmq_socket_t tmq_tcp_socket();
int tmq_socket_nonblocking(tmq_socket_t fd);
void tmq_socket_close(tmq_socket_t fd);
int tmq_socket_reuse_addr(tmq_socket_t fd, int enable);

#endif //TINYMQTT_MQTT_SOCKET_H
