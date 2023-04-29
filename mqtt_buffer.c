//
// Created by zr on 23-4-9.
//
#include "mqtt_buffer.h"
#include "mqtt_vec.h"
#include "tlog.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <stdio.h>

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
    tmq_buffer_chunk_t* chunk = NULL;
    size_t free_list_idx = FREE_LIST_INDEX(req_size);
    for(; free_list_idx < 5; free_list_idx++)
    {
        tmq_buffer_chunk_t** next = &buffer->free_chunk_list[free_list_idx];
        while(*next && (*next)->chunk_size < req_size)
            next = &(*next)->next;
        if(!*next)
            continue;
        chunk = *next;
        *next = (*next)->next;
        chunk->next = NULL;
        break;
    }
    return chunk;
}

static tmq_buffer_chunk_t* get_largest_free_chunk(tmq_buffer_t* buffer)
{
    int i = 4;
    while(i > 0 && !buffer->free_chunk_list[i])
        i--;
    if(i < 0) return NULL;
    tmq_buffer_chunk_t* largest = buffer->free_chunk_list[i];
    if(i == 0)
    {
        buffer->free_chunk_list[0] = largest->next;
        largest->next = NULL;
    }
    else
    {
        tmq_buffer_chunk_t** next = &buffer->free_chunk_list[i];
        while((*next)->next)
            next = &(*next)->next;
        largest = *next;
        *next = NULL;
    }
    return largest;
}

void tmq_buffer_append(tmq_buffer_t* buffer, const char* data, size_t size)
{
    if(!buffer || !data || !size) return;
    tmq_buffer_chunk_t* chunk = buffer->last;
    if(!chunk)
    {
        chunk = find_free_chunk(buffer, size);
        if(!chunk)
            chunk = buffer_chunk_new(size);
        memcpy(chunk->buf, data, size);
        chunk->write_idx += size;
        buffer->first = buffer->last = chunk;
    }
    else if(CHUNK_WRITEABLE(chunk) >= size)
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

void tmq_buffer_prepend(tmq_buffer_t* buffer, const char* data, size_t size)
{
    tmq_buffer_chunk_t* chunk = buffer->first;
    if(!chunk)
    {
        chunk = find_free_chunk(buffer, size);
        if(!chunk)
            chunk = buffer_chunk_new(size);
        memcpy(chunk->buf, data, size);
        chunk->write_idx += size;
        buffer->first = buffer->last = chunk;
    }
    else if(chunk->read_idx >= size)
    {
        chunk->read_idx -= size;
        memcpy(chunk->buf + chunk->read_idx, data, size);
    }
    else
    {
        size_t remain = size - chunk->read_idx;
        tmq_buffer_chunk_t* new_chunk = find_free_chunk(buffer, remain);
        if(!new_chunk)
            new_chunk = buffer_chunk_new(remain);
        if(!new_chunk) return;
        size_t align = new_chunk->chunk_size - remain;
        new_chunk->read_idx += align;
        memcpy(new_chunk->buf + new_chunk->read_idx, data, remain);
        new_chunk->write_idx += new_chunk->read_idx + remain;
        data += remain;
        size -= remain;
        if(size)
        {
            chunk->read_idx -= size;
            memcpy(chunk->buf + chunk->read_idx, data, size);
        }
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
    if(remove)
    {
        buffer->first = chunk;
        if(!chunk)
            buffer->last = NULL;
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
    buffer->first = chunk;
    if(!chunk)
        buffer->last = NULL;
    buffer->readable_bytes -= size;
}

ssize_t tmq_buffer_read_fd(tmq_buffer_t* buffer, tmq_socket_t fd, size_t max)
{
    if(!buffer) return 0;
    int fd_readable = 0;
    if(ioctl(fd, FIONREAD, &fd_readable) < 0)
    {
        tlog_error("ioctl() error %d: %s", errno, strerror(errno));
        return 0;
    }
    size_t size;
    if(!max)
        size = min(fd_readable, FD_MAX_READ_BYTES);
    else
        size = min(fd_readable, min(max, FD_MAX_READ_BYTES));
    int iovec_cnt = 0;
    size_t space_aval = 0;
    struct iovec vecs[MAX_IOVEC_NUM];
    tmq_buffer_chunk_t* chunk = buffer->last;
    if(chunk && CHUNK_WRITEABLE(chunk) > 0)
    {
        vecs[0].iov_base = chunk->buf + chunk->write_idx;
        size_t len = CHUNK_WRITEABLE(chunk) <= size ? CHUNK_WRITEABLE(chunk) : size;
        vecs[0].iov_len = len;
        iovec_cnt++;
        chunk->write_idx += len;
        space_aval += CHUNK_WRITEABLE(chunk);
    }
    for(int i = iovec_cnt; space_aval < size && i < MAX_IOVEC_NUM; i++)
    {
        chunk = find_free_chunk(buffer, size - space_aval);
        if(!chunk && i < MAX_IOVEC_NUM - 1)
            chunk = get_largest_free_chunk(buffer);
        if(!chunk)
            chunk = buffer_chunk_new(size - space_aval);
        assert(chunk && chunk->chunk_size > 0);
        if(!buffer->last)
            buffer->first = buffer->last = chunk;
        else
        {
            buffer->last->next = chunk;
            buffer->last = chunk;
        }
        size_t len = space_aval + chunk->chunk_size <= size ? chunk->chunk_size : size - space_aval;
        space_aval += chunk->chunk_size;
        vecs[i].iov_base = chunk->buf;
        vecs[i].iov_len = len;
        iovec_cnt++;
        chunk->write_idx += len;
    }
    ssize_t n = readv(fd, vecs, iovec_cnt);
    if(n > 0)
        buffer->readable_bytes += n;
    return n;
}

ssize_t tmq_buffer_write_fd(tmq_buffer_t* buffer, tmq_socket_t fd)
{
    if(!buffer) return 0;
    tmq_vec(struct iovec) vecs = tmq_vec_make(struct iovec);
    tmq_buffer_chunk_t* chunk = buffer->first;
    int iovec_cnt = 0;
    while (chunk)
    {
        struct iovec iov = {
                .iov_base = chunk->buf + chunk->read_idx,
                .iov_len = CHUNK_DATA_LEN(chunk)
        };
        tmq_vec_push_back(vecs, iov);
        chunk = chunk->next;
    }
    ssize_t n = writev(fd, tmq_vec_begin(vecs), iovec_cnt);
    tmq_vec_free(vecs);
    if(n < 0)
        return n;
    tmq_buffer_remove(buffer, n);
    return n;
}

void tmq_buffer_free(tmq_buffer_t* buffer)
{
    if(!buffer) return;
    tmq_buffer_chunk_t* chunk, *next;
    for(int i = 0; i < 5; i++)
    {
        chunk = buffer->free_chunk_list[i];
        while(chunk)
        {
            next = chunk->next;
            free(chunk);
            chunk = next;
        }
    }
    chunk = buffer->first;
    while(chunk)
    {
        next = chunk->next;
        free(chunk);
        chunk = next;
    }
}

void tmq_buffer_debug(const tmq_buffer_t* buffer)
{
    if(!buffer) return;
    printf("buffer %p: readable bytes=[%lu]\n", buffer, buffer->readable_bytes);
    printf("---------------------------------------\n");
    printf("chunks in use:\n");
    tmq_buffer_chunk_t* chunk = buffer->first;
    int used_chunks = 0;
    size_t used_chunk_size = 0;
    while (chunk)
    {
        used_chunks++;
        printf("chunk %d: chunk size=[%lu] read_idx=[%lu] write_idx=[%lu] data len=[%lu] writable space=[%lu]\n",
               used_chunks, chunk->chunk_size,
               chunk->read_idx, chunk->write_idx,
               CHUNK_DATA_LEN(chunk), CHUNK_WRITEABLE(chunk));
        used_chunk_size += chunk->chunk_size;
        chunk = chunk->next;
    }
    printf("total %d chunk in use, total size=[%lu]\n", used_chunks, used_chunk_size);
    printf("---------------------------------------\n");
    printf("chunks in free list:\n");
    for(int i = 0; i < 5; i++)
    {
        switch (i)
        {
            case 0: printf("[512]:\n"); break;
            case 1: printf("(512, 1204]:\n"); break;
            case 2: printf("(1024, 2048]:\n"); break;
            case 3: printf("(2048, 4096]:\n"); break;
            case 4: printf("(4096, ):\n"); break;
            default: break;
        }
        chunk = buffer->free_chunk_list[i];
        while(chunk)
        {
            printf("free chunk: chunk size=[%lu]\n", chunk->chunk_size);
            chunk = chunk->next;
        }
    }
}