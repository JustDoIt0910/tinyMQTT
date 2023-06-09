//
// Created by zr on 23-4-14.
//
#include "mqtt_vec.h"
#include <stdlib.h>
#include <string.h>

tmq_vec_base_t* tmq_base_init_(size_t elem_size)
{
    tmq_vec_base_t* base = malloc(sizeof(tmq_vec_base_t));
    if(!base)
        return NULL;
    memset(base, 0, sizeof(tmq_vec_base_t));
    base->elem_size = elem_size;
    return base;
}

static void* index_to_addr(tmq_vec_base_t* v, size_t index)
{
    return (char*)v->data + index * v->elem_size;
}

static int tmq_vec_grow(tmq_vec_base_t* v)
{
    size_t cap = v->cap < 8 ? 8 : v->cap * 2;
    void* data = realloc(v->data, cap * v->elem_size);
    if(!data)
        return -1;
    v->data = data;
    v->cap = cap;
    return 0;
}

int tmq_vec_push_back_(tmq_vec_base_t* v, const void* elem)
{
    if(!v) return -1;
    if(v->size >= v->cap)
    {
        if(tmq_vec_grow(v) < 0)
            return -1;
    }
    memcpy(index_to_addr(v, v->size), elem, v->elem_size);
    v->size++;
    return 0;
}

void* tmq_vec_pop_back_(tmq_vec_base_t* v)
{
    if(!v || v->size == 0) return NULL;
    void* ret = tmq_vec_get_(v, v->size - 1);
    v->size--;
    return ret;
}

int tmq_vec_insert_(tmq_vec_base_t* v, size_t index, const void* elem)
{
    if(!v || index > v->size)
        return -1;
    if(v->size >= v->cap)
    {
        if(tmq_vec_grow(v) < 0)
            return -1;
    }
    memmove(index_to_addr(v, index + 1),
            index_to_addr(v, index),
            (v->size - index) * v->elem_size);
    memcpy(index_to_addr(v, index), elem, v->elem_size);
    v->size++;
    return 0;
}

int tmq_vec_erase_(tmq_vec_base_t* v, size_t index)
{
    if(!v || index >= v->size)
        return -1;
    memmove(index_to_addr(v, index),
            index_to_addr(v, index + 1),
            (v->size - index - 1) * v->elem_size);
    v->size--;
}

void* tmq_vec_get_(tmq_vec_base_t* v, size_t index)
{
    if(!v || index >= v->size)
        return NULL;
    return index_to_addr(v, index);
}

int tmq_vec_set_(tmq_vec_base_t* v, size_t index, const void* elem)
{
    void* p = tmq_vec_get_(v, index);
    if(!p) return -1;
    memcpy(p, elem, v->elem_size);
    return 0;
}

void* tmq_vec_begin_(tmq_vec_base_t* v)
{
    if(!v) return NULL;
    return v->data;
}

void* tmq_vec_end_(tmq_vec_base_t* v)
{
    if(!v)
        return NULL;
    return index_to_addr(v, v->size);
}

void tmq_vec_clear_(tmq_vec_base_t* v) {v->size = 0;}

void tmq_vec_free_(tmq_vec_base_t* v)
{
    if(!v) return;
    if(v->data)
    {
        free(v->data);
        v->data = NULL;
    }
    free(v);
}

size_t tmq_vec_size_(tmq_vec_base_t* v)
{
    if(!v) return 0;
    return v->size;
}

int tmq_vec_empty_(tmq_vec_base_t* v)
{
    if(!v) return 1;
    return v->size == 0;
}

int tmq_vec_resize_(tmq_vec_base_t* v, size_t size)
{
    if(!v) return -1;
    if(tmq_vec_reserve_(v, size) < 0)
        return -1;
    v->size = size;
    return 0;
}

int tmq_vec_reserve_(tmq_vec_base_t* v, size_t size)
{
    if(size > v->cap)
    {
        size_t cap = 2 * size;
        void* data = realloc(v->data, cap * v->elem_size);
        if(!data)
            return -1;
        v->data = data;
        v->cap = cap;
    }
}

void tmq_vec_swap_(tmq_vec_base_t** v1, tmq_vec_base_t** v2)
{
    if((*v1)->elem_size != (*v2)->elem_size)
        return;
    tmq_vec_base_t* tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
}