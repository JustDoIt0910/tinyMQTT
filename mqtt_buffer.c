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
    bzero(buffer->free_chunk_list, 5 * sizeof(tmq_buffer_chunk_t*));
}

static tmq_buffer_chunk_t* buffer_chunk_new(size_t size)
{
    size = size < BUFFER_CHUNK_MIN ? BUFFER_CHUNK_MIN : size;
    tmq_buffer_chunk_t* chunk = (tmq_buffer_chunk_t*) malloc(sizeof(tmq_buffer_chunk_t) + size);
    if(!chunk)
    {
        tlog_error("buffer_chunk_new(): out of memory");
        return NULL;
    }
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

static tmq_buffer_chunk_t* find_free_chunk(tmq_buffer_t* buffer, size_t req_size)
{
    size_t free_list_idx = FREE_LIST_INDEX(req_size);
    for(; free_list_idx < 5; free_list_idx++)
    {
        tmq_buffer_chunk_t* prev = NULL, *chunk = buffer->free_chunk_list[free_list_idx];
        while(chunk && chunk->chunk_size < req_size)
        {
            prev = chunk;
            chunk = chunk->next;
        }
        if(chunk)
        {
            if(!prev)
                buffer->free_chunk_list[free_list_idx] = chunk->next;
            else
                prev->next = chunk->next;
            chunk->next = NULL;
            return chunk;
        }
    }
    return NULL;
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
        tmq_buffer_chunk_t* new_chunk = find_free_chunk(buffer, remain);
        if(!new_chunk)
            new_chunk = buffer_chunk_new(remain);
        if(!new_chunk) return;
        memcpy(new_chunk->buf, data, remain);
        new_chunk->write_idx += remain;
        chunk->next = new_chunk;
        buffer->last = new_chunk;
    }
    buffer->readable_bytes += size;
}

static void buffer_chunk_remove(tmq_buffer_t* buffer, tmq_buffer_chunk_t* chunk)
{
    chunk->next = NULL;
    chunk->read_idx = chunk->write_idx = 0;
    size_t free_list_idx = FREE_LIST_INDEX(chunk->chunk_size);
    if(!buffer->free_chunk_list[free_list_idx])
        buffer->free_chunk_list[free_list_idx] = chunk;
    else
    {
        tmq_buffer_chunk_t* p = buffer->free_chunk_list[free_list_idx];
        if(p->chunk_size >= chunk->chunk_size)
        {
            chunk->next = p;
            buffer->free_chunk_list[free_list_idx] = chunk;
        }
        else
        {
            while(p->next && p->next->chunk_size < chunk->chunk_size)
                p = p->next;
            chunk->next = p->next;
            p->next = chunk;
        }
    }
}

static size_t buffer_read_internal(tmq_buffer_t* buffer, char* buf, size_t size, int remove)
{
    if(!buffer || !buf || !size) return 0;
    if(size > buffer->readable_bytes)
    {
        tlog_warn("buffer_read_internal(): buffer->redable_bytes < size");
        size = buffer->readable_bytes;
    }
    tmq_buffer_chunk_t* chunk = buffer->first, *next;
    if(!chunk) return 0;
    size_t cnt = 0;
    while(chunk && size >= CHUNK_DATA_LEN(chunk))
    {
        memcpy(buf, chunk->buf + chunk->read_idx, CHUNK_DATA_LEN(chunk));
        size -= CHUNK_DATA_LEN(chunk);
        buf += CHUNK_DATA_LEN(chunk);
        cnt += CHUNK_DATA_LEN(chunk);
        if(!remove)
        {
            chunk = chunk->next;
            continue;
        }
        next = chunk->next;
        buffer_chunk_remove(buffer, chunk);
        chunk = next;
    }
    if(chunk && size)
    {
        memcpy(buf, chunk->buf + chunk->read_idx, size);
        cnt += size;
        if(remove)
            chunk->read_idx += size;
    }
    return cnt;
}

size_t tmq_buffer_peek(tmq_buffer_t* buffer, char* buf, size_t size)
{
    return buffer_read_internal(buffer, buf, size, 0);
}

size_t tmq_buffer_read(tmq_buffer_t* buffer, char* buf, size_t size)
{
    size_t n = buffer_read_internal(buffer, buf, size, 1);
    buffer->readable_bytes -= n;
    return n;
}

void tmq_buffer_remove(tmq_buffer_t* buffer, size_t size)
{
    if(!buffer || !size) return;
    if(size > buffer->readable_bytes)
    {
        tlog_warn("tmq_buffer_remove(): buffer->redable_bytes < size");
        size = buffer->readable_bytes;
    }
    tmq_buffer_chunk_t* chunk = buffer->first, *next;
    if(!chunk) return;
    while(chunk && size >= CHUNK_DATA_LEN(chunk))
    {
        size -= CHUNK_DATA_LEN(chunk);
        next = chunk->next;
        buffer_chunk_remove(buffer, chunk);
        chunk = next;
    }
    if(chunk && size)
        chunk->read_idx += size;
}