//
// Created by zr on 23-4-9.
//
#include "mqtt_buffer.h"
#include <stdlib.h>
#include <string.h>

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

    }
}