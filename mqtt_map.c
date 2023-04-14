//
// Created by zr on 23-4-9.
//
#include "mqtt_map.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

/* using murmurhash for str hashing */
unsigned hash_str(const void* key)
{
    const char* strkey = *(const char**)key;
    const uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
    const size_t len = strlen(strkey);
    const unsigned char *p = (const unsigned char *) strkey;
    const unsigned char *end = p + (len & ~(uint64_t) 0x7);
    uint64_t h = (len * m);

    while (p != end) {
        uint64_t k;
        memcpy(&k, p, sizeof(k));

        k *= m;
        k ^= k >> 47u;
        k *= m;

        h ^= k;
        h *= m;
        p += 8;
    }

    switch (len & 7u) {
        case 7:
            h ^= (uint64_t) p[6] << 48ul; // fall through
        case 6:
            h ^= (uint64_t) p[5] << 40ul; // fall through
        case 5:
            h ^= (uint64_t) p[4] << 32ul; // fall through
        case 4:
            h ^= (uint64_t) p[3] << 24ul; // fall through
        case 3:
            h ^= (uint64_t) p[2] << 16ul; // fall through
        case 2:
            h ^= (uint64_t) p[1] << 8ul; // fall through
        case 1:
            h ^= (uint64_t) p[0]; // fall through
            h *= m;
        default:
            break;
    }

    h ^= h >> 47u;
    h *= m;
    h ^= h >> 47u;

    return (uint32_t) h;
}

int equal_str(const void* k1, const void* k2)
{
    const char* s1 = *(const char**)k1;
    const char* s2 = *(const char**)k2;
    return !strcmp(s1, s2);
}

unsigned hash_32(const void* key) {return *(const uint32_t*)key;}

int equal_32(const void* k1, const void* k2)
{
    uint32_t a = *(const uint32_t*)k1;
    uint32_t b = *(const uint32_t*)k2;
    return a == b;
}

unsigned hash_64(const void* key)
{
    uint64_t k = *(const uint64_t*)key;
    return ((uint32_t) k) ^ (uint32_t)(k >> 32u);
}

int equal_64(const void* k1, const void* k2)
{
    uint64_t a = *(const uint64_t*)k1;
    uint64_t b = *(const uint64_t*)k2;
    return a == b;
}

static tmq_map_entry_t* tmq_map_entry_new_(tmq_map_base_t* m, const void* key, const void* value)
{
    assert(!(m->key_type != KEY_TYPE_STR && m->key_size == 0));
    assert(m->value_size != 0);
    size_t key_size = m->key_type == KEY_TYPE_STR ? strlen(*(const char**)key) + 1: m->key_size;
    size_t s1 = sizeof(tmq_map_entry_t) + key_size;
    size_t s1_aligned = (s1 + m->value_size - 1) & ~(m->value_size - 1);
    size_t v_offset = s1_aligned - s1;
    size_t total = s1_aligned + m->value_size;
    tmq_map_entry_t* entry = (tmq_map_entry_t*) malloc(total);
    if(!entry)
        return NULL;
    entry->next = NULL;
    entry->hash = m->hash_fn(key);
    entry->key = (void*)(entry + 1);
    entry->value = ((char*)(entry + 1)) + v_offset + key_size;
    if(m->key_type != KEY_TYPE_STR)
        memcpy(entry->key, key, key_size);
    else
        memcpy(entry->key, *(const char**)key, key_size);
    memcpy(entry->value, value, m->value_size);
    return entry;
}

static tmq_map_entry_t** tmq_map_alloc_buckets(uint32_t* cap, uint32_t grow_factor)
{
    if(*cap > (4294967295U) / grow_factor)
        return NULL;
    uint32_t v = *cap < 8 ? 8 :  *cap * grow_factor;
    /* make sure that the real cap is power of 2,
     * so we can use bitwise operation instead of mod */
    v--;
    for(int i = 1; i < sizeof(uint32_t) * 8; i *= 2)
        v |= v >> i;
    v++;
    tmq_map_entry_t** buckets = (tmq_map_entry_t**)malloc(sizeof(tmq_map_entry_t*) * v);
    if(!buckets) return NULL;
    memset(buckets, 0, v * sizeof(tmq_map_entry_t*));
    *cap = v;
    return buckets;
}

static size_t tmq_map_bucket_index(tmq_map_base_t* m, unsigned hash)
{
    /* if cap == 2^n, then hash % cap == hash & (cap - 1),
     * but bitwise operation is much faster */
    return hash & (m->cap - 1);
}

static tmq_map_entry_t* tmq_map_find_entry(tmq_map_base_t* m, const void* key)
{
    unsigned hash = m->hash_fn(key);
    size_t bucket_idx = tmq_map_bucket_index(m, hash);
    assert(bucket_idx < m->cap);
    tmq_map_entry_t* entry = m->buckets[0][bucket_idx];
    while(entry)
    {
        const void* entry_key = m->key_type == KEY_TYPE_STR ? &entry->key : entry->key;
        /* using hash in entry to speed up the finding */
        if(hash == entry->hash && m->equal_fn(entry_key, key))
            return entry;
        entry = entry->next;
    }
    return NULL;
}

static void tmq_map_insert_entry(tmq_map_base_t* m, tmq_map_entry_t* entry, tmq_map_entry_t** buckets)
{
    size_t bucket_idx = tmq_map_bucket_index(m, entry->hash);
    assert(bucket_idx < m->cap);
    entry->next = buckets[bucket_idx];
    buckets[bucket_idx] = entry;
}

static int tmq_map_grow(tmq_map_base_t* m)
{
    if(m->size < m->remap_thresh)
        return 0;
    uint32_t old_cap = m->cap;
    tmq_map_entry_t** buckets = tmq_map_alloc_buckets(&m->cap, 2);
    if(!buckets)
        return -1;
    m->buckets[1] = buckets;
    /* remap */
    tmq_map_entry_t* entry, *next;
    for(int i = 0; i < old_cap; i++)
    {
        entry = m->buckets[0][i];
        while(entry)
        {
            next = entry->next;
            tmq_map_insert_entry(m, entry, m->buckets[1]);
            entry = next;
        }
    }
    free(m->buckets[0]);
    m->buckets[0] = m->buckets[1];
    m->buckets[1] = NULL;
    m->remap_thresh = (uint32_t)(m->cap * m->load_fac / 100);
//    printf("map resize\n");
//    printf("previous cap: %u new cap: %u\n", old_cap, m->cap);
    return 0;
}

tmq_map_base_t* tmq_map_new_(uint32_t cap, uint32_t factor,
                        size_t key_size, size_t value_size, unsigned char key_type,
                        tmq_map_hash_f hash_fn, tmq_map_equal_f equal_fn)
{
    assert(key_type == KEY_TYPE_STR || key_type == KEY_TYPE_BUILTIN
    || key_type == KEY_TYPE_CUSTOM);
    if(factor > 95 || factor < 25)
        return NULL;
    if(!hash_fn || !equal_fn)
        return NULL;
    assert(!(key_type != KEY_TYPE_STR && key_size == 0));
    tmq_map_base_t* m = (tmq_map_base_t*) malloc(sizeof(tmq_map_base_t));
    if(!m) return NULL;
    m->load_fac = factor;
    m->hash_fn = hash_fn;
    m->equal_fn = equal_fn;
    m->key_size = key_size;
    m->value_size = value_size;
    m->key_type = key_type;
    m->buckets[0] = tmq_map_alloc_buckets(&cap, 1);
    m->buckets[1] = NULL;
    if(!m->buckets[0])
    {
        free(m);
        return NULL;
    }
    m->cap = cap;
    m->remap_thresh = (uint32_t)(m->cap * factor / 100);
//    printf("map cap = %u\n", m->cap);
    return m;
}

int tmq_map_put_(tmq_map_base_t* m, const void* key, const void* value)
{
    if(!key || !value)
        return -1;
    tmq_map_entry_t* entry = tmq_map_find_entry(m, key);
    if(entry)
    {
        memcpy(entry->value, value, m->value_size);
        return 0;
    }
    entry = tmq_map_entry_new_(m, key, value);
    if(!entry)
        return -1;
    if(m->size >= m->remap_thresh)
        tmq_map_grow(m);
    tmq_map_insert_entry(m, entry, m->buckets[0]);
    m->size++;
    return 0;
}

void* tmq_map_get_(tmq_map_base_t* m, const void* key)
{
    tmq_map_entry_t* entry = tmq_map_find_entry(m, key);
    if(entry)
        return entry->value;
    return NULL;
}

void tmq_map_free_(tmq_map_base_t* m)
{
    tmq_map_entry_t* entry, *next;
    for(int i = 0; i < m->cap; i++)
    {
        entry = m->buckets[0][i];
        while(entry)
        {
            next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(m->buckets[0]);
}