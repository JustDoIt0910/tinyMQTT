//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_BUFFER_H
#define TINYMQTT_MQTT_BUFFER_H
#include <stddef.h>
#include "base/mqtt_socket.h"

#define BUFFER_CHUNK_MIN            512
#define FD_MAX_READ_BYTES           65536
#define CHUNK_DATA_LEN(chunk)       ((chunk)->write_idx - (chunk)->read_idx)
#define CHUNK_WRITEABLE(chunk)      ((chunk)->chunk_size - (chunk)->write_idx)
#define CHUNK_AVAL_SPACE(chunk)     ((chunk)->chunk_size - ((chunk)->write_idx - (chunk)->read_idx))
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
    int chunks;
    tmq_buffer_chunk_t* first;
    tmq_buffer_chunk_t* last;
    size_t readable_bytes;
} tmq_buffer_t;

void tmq_buffer_init(tmq_buffer_t* buffer);
void tmq_buffer_append(tmq_buffer_t* buffer, const char* data, size_t size);
void tmq_buffer_prepend(tmq_buffer_t* buffer, const char* data, size_t size);
size_t tmq_buffer_peek(tmq_buffer_t* buffer, char* buf, size_t size);
size_t tmq_buffer_read(tmq_buffer_t* buffer, char* buf, size_t size);
ssize_t tmq_buffer_read_fd(tmq_buffer_t* buffer, tmq_socket_t fd, size_t max);
ssize_t tmq_buffer_write_fd(tmq_buffer_t* buffer, tmq_socket_t fd);
void tmq_buffer_remove(tmq_buffer_t* buffer, size_t size);
void tmq_buffer_free(tmq_buffer_t* buffer);
void tmq_buffer_debug(const tmq_buffer_t* buffer);
/* functions below will automatically convert network endian to host endian */
void tmq_buffer_peek16(tmq_buffer_t* buffer, uint16_t* v);
void tmq_buffer_peek32(tmq_buffer_t* buffer, uint32_t* v);
void tmq_buffer_peek64(tmq_buffer_t* buffer, uint64_t* v);
void tmq_buffer_read16(tmq_buffer_t* buffer, uint16_t* v);
void tmq_buffer_read32(tmq_buffer_t* buffer, uint32_t* v);
void tmq_buffer_read64(tmq_buffer_t* buffer, uint64_t* v);

#endif //TINYMQTT_MQTT_BUFFER_H
