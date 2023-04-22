//
// Created by zr on 23-4-9.
//
#include "mqtt_buffer.h"
#include "tlog.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void tmq_buffer_init(tmq_buffer_t* buffer)
{
    if(!buffer) return;
    buffer->first = buffer->last = NULL;
    buffer->readable_bytes = 0;
}

static tmq_buffer_chunk_t* buffer_chunk_new(size_t size)
{
    size = size < BUFFER_CHUNK_MIN ? BUFFER_CHUNK_MIN : size;
    tmq_buffer_chunk_t* chunk = (tmq_buffer_chunk_t*) malloc(sizeof(tmq_buffer_chunk_t) + size);
    if(!chunk) return NULL;
    chunk->next = NULL;
    chunk->read_idx = chunk->write_idx = 0;
    chunk->chunk_size = size;
    return chunk;
}

static void buffer_chunk_realign(tmq_buffer_chunk_t* chunk)
{
    if(chunk->read_idx == 0) return;
    memmove(chunk->buf, chunk->buf + chunk->read_idx, CHUNK_DATA_LEN(chunk));
    chunk->write_idx -= chunk->read_idx;
    chunk->read_idx = 0;
}

void tmq_buffer_append(tmq_buffer_t* buffer, const char* data, size_t size)
{
    if(!buffer || !data || !size) return;
    tmq_buffer_chunk_t* chunk = buffer->last;
    if(!chunk)
    {
        chunk = buffer_chunk_new(size);
        memcpy(chunk->buf, data, size);
        chunk->write_idx += size;
        buffer->first = buffer->last = chunk;
        return;
    }
    if(CHUNK_WRITEABLE(chunk) >= size)
    {
        memcpy(chunk->buf + chunk->write_idx, data, size);
        chunk->write_idx += size;
    }
    else if(CHUNK_AVAL_SPACE(chunk) >= size)
    {
        buffer_chunk_realign(chunk);
        assert(CHUNK_WRITEABLE(chunk) >= size);
        memcpy(chunk->buf + chunk->write_idx, data, size);
        chunk->write_idx += size;
    }
    else
    {
        size_t last_writable = CHUNK_WRITEABLE(chunk);
        memcpy(chunk->buf + chunk->write_idx, data, last_writable);
        chunk->write_idx += last_writable;
        data += last_writable;
        size_t remain = size - last_writable;
        tmq_buffer_chunk_t* new_chunk = buffer_chunk_new(remain);
        memcpy(new_chunk->buf, data, remain);
        new_chunk->write_idx += remain;
        chunk->next = new_chunk;
        buffer->last = new_chunk;
    }
    buffer->readable_bytes += size;
}

size_t tmq_buffer_peek(tmq_buffer_t* buffer, char* buf, size_t size)
{
    if(!buffer || !buf || !size) return 0;
    if(size > buffer->readable_bytes)
    {
        tlog_warn("tmq_buffer_peek(): buffer->redable_bytes < size");
        size = buffer->readable_bytes;
    }
    tmq_buffer_chunk_t* chunk = buffer->first;
    if(!chunk) return 0;
    while(chunk && size >= CHUNK_DATA_LEN(chunk))
    {
        
        size -= CHUNK_DATA_LEN(chunk);
        chunk = chunk->next;
    }
}