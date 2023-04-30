//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_BUFFER_H
#define TINYMQTT_MQTT_BUFFER_H
#include <stddef.h>
#include "mqtt_socket.h"

#define MAX_IOVEC_NUM               4
#define BUFFER_CHUNK_MIN            512
#define FD_MAX_READ_BYTES           65536
#define CHUNK_DATA_LEN(chunk)       ((chunk)->write_idx - (chunk)->read_idx)
#define CHUNK_WRITEABLE(chunk)      ((chunk)->chunk_size - (chunk)->write_idx)
#define CHUNK_AVAL_SPACE(chunk)     ((chunk)->chunk_size - ((chunk)->write_idx - (chunk)->read_idx))
#define FREE_LIST_INDEX(size)       ((((size) - 1) >> 9) > 0) + ((((size) - 1) >> 10) > 0) + \
                                    ((((size) - 1) >> 11) > 0) + ((((size) - 1) >> 12) > 0)
#define min(a, b)                   (((a) < (b)) ? (a) : (b))

typedef struct tmq_buffer_chunk_s
{
    struct tmq_buffer_chunk_s* next;
    size_t chunk_size;
    size_t read_idx;
    size_t write_idx;
    char buf[];
} tmq_buffer_chunk_t;

typedef struct tmq_buffer_s
{
    tmq_buffer_chunk_t* first;
    tmq_buffer_chunk_t* last;
    /* free list 512B, (512B, 1KB], (1KB, 2KB], (2KB, 4KB], (4KB, ) */
    tmq_buffer_chunk_t* free_chunk_list[5];
    size_t readable_bytes;
} tmq_buffer_t;

void tmq_buffer_init(tmq_buffer_t* buffer);
void tmq_buffer_append(tmq_buffer_t* buffer, const char* data, size_t size);
void tmq_buffer_prepend(tmq_buffer_t* buffer, const char* data, size_t size);
size_t tmq_buffer_peek(tmq_buffer_t* buffer, char* buf, size_t size);
size_t tmq_buffer_read(tmq_buffer_t* buffer, char* buf, size_t size);
void tmq_buffer_remove(tmq_buffer_t* buffer, size_t size);
ssize_t tmq_buffer_read_fd(tmq_buffer_t* buffer, tmq_socket_t fd, size_t max);
ssize_t tmq_buffer_write_fd(tmq_buffer_t* buffer, tmq_socket_t fd);
void tmq_buffer_free(tmq_buffer_t* buffer);
void tmq_buffer_debug(const tmq_buffer_t* buffer);

#endif //TINYMQTT_MQTT_BUFFER_H
