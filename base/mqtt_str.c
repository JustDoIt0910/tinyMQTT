//
// Created by zr on 23-5-19.
//
#include "mqtt_str.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

tmq_str_t tmq_str_new_len(const char* data, size_t len)
{
    size_t hdr_len = sizeof(tmq_ds_t);
    tmq_ds_t* hdr = malloc(hdr_len + len + 1);
    if(!hdr)
        return NULL;
    char* s = (char*)hdr + hdr_len;
    memset(s, 0, len + 1);
    if(data && len)
        memcpy(s, data, len);
    hdr->len = len;
    hdr->alloc = hdr_len + len + 1;
    return s;
}

tmq_str_t tmq_str_new(const char* data)
{
    size_t len = data ? strlen(data): 0;
    return tmq_str_new_len(data, len);
}

tmq_str_t tmq_str_empty() { return tmq_str_new_len("", 0); }

size_t tmq_str_len(tmq_str_t s)
{
    if(!s) return 0;
    tmq_ds_t* hdr = TMQ_DS_HDR(s);
    return hdr->len;
}

static tmq_str_t tmq_str_grow(tmq_str_t s, size_t len)
{
    size_t hdr_len = sizeof(tmq_ds_t);
    size_t new_alloc = 2 * len + hdr_len + 1;
    tmq_ds_t* hdr = TMQ_DS_HDR(s);
    tmq_ds_t* tmp = realloc(hdr, new_alloc);
    if(!tmp)
        return NULL;
    hdr = tmp;
    hdr->alloc = new_alloc;
    s = (char*)hdr + hdr_len;
    return s;
}

tmq_str_t tmq_str_append_char(tmq_str_t s, char c)
{
    if(!s) return NULL;
    tmq_ds_t* hdr = TMQ_DS_HDR(s);
    hdr->buf[hdr->len] = c;
    size_t hdr_len = sizeof(tmq_ds_t);
    size_t new_len = hdr->len + 1;
    if(hdr_len + new_len + 1 > hdr->alloc)
    {
        tmq_str_t tmp = tmq_str_grow(s, new_len);
        if(!tmp)
            return s;
        s = tmp;
    }
    hdr = TMQ_DS_HDR(s);
    hdr->len += 1;
    hdr->buf[hdr->len] = 0;
    return s;
}

tmq_str_t tmq_str_append_str(tmq_str_t s, const char* str)
{
    if(!s || !str) return s;
    return tmq_str_append_data_n(s, str, strlen(str));
}

tmq_str_t tmq_str_append_data_n(tmq_str_t s, const char* data, size_t n)
{
    if(!data || !n) return s;
    tmq_ds_t* hdr = TMQ_DS_HDR(s);
    size_t hdr_len = sizeof(tmq_ds_t);
    size_t new_len = hdr->len + n;
    if(hdr_len + new_len + 1 > hdr->alloc)
    {
        tmq_str_t tmp = tmq_str_grow(s, new_len);
        if(!tmp)
            return s;
        s = tmp;
    }
    hdr = TMQ_DS_HDR(s);
    memcpy(hdr->buf + hdr->len, data, n);
    hdr->len = new_len;
    hdr->buf[hdr->len] = 0;
    return s;
}

void tmq_str_free(tmq_str_t s)
{
    if(!s) return;
    tmq_ds_t* hdr = TMQ_DS_HDR(s);
    free(hdr);
}

void tmq_str_debug(tmq_str_t s)
{
    if(!s) return;
    tmq_ds_t* hdr = TMQ_DS_HDR(s);
    printf("addr = %p len = %zu alloc = %zu\n", s, hdr->len, hdr->alloc);
    printf("content = %s\n", s);
}

void tmq_str_clear(tmq_str_t s)
{
    if(!s) return;
    tmq_ds_t* hdr = TMQ_DS_HDR(s);
    memset(s, 0, hdr->len);
    hdr->len = 0;
}

tmq_str_t tmq_str_assign(tmq_str_t s, const char* str)
{
    if(!s) s = tmq_str_new(NULL);
    tmq_str_clear(s);
    return tmq_str_append_str(s, str);
}

tmq_str_t tmq_str_assign_n(tmq_str_t s, const char* data, size_t n)
{
    if(!s) s = tmq_str_new(NULL);
    tmq_str_clear(s);
    return tmq_str_append_data_n(s, data, n);
}

tmq_str_t tmq_str_parse_int(int64_t v, int base)
{
    if(base < 2 || base > 16)
        return NULL;
    tmq_str_t str = tmq_str_empty();
    if(!str) return NULL;
    static char tbl[] = "0123456789ABCDEF";
    char tmp[100] = {0};
    int64_t s = v; int i = 0;
    if(s < 0)
    {
        s = -s;
        str = tmq_str_append_char(str, '-');
    }
    if(s == 0)
    {
        str = tmq_str_append_char(str, '0');
        return str;
    }
    while(s > 0)
    {
        tmp[i++] = tbl[s % base];
        s /= base;
    }
    for(int j = i - 1; j >= 0; j--)
        str = tmq_str_append_char(str, tmp[j]);
    return str;
}

int tmq_str_to_int(tmq_str_t s, int64_t* v)
{
    char* failed_ptr = NULL;
    int64_t integer = strtoll(s, &failed_ptr, 10);
    if(failed_ptr && *failed_ptr)
        return 0;
    if(v)
        *v = integer;
    return 1;
}

char tmq_str_at(tmq_str_t s, size_t index)
{
    tmq_ds_t* hdr = TMQ_DS_HDR(s);
    if(index >= hdr->len)
        return -1;
    return s[index];
}

int tmq_str_equal(tmq_str_t s1, tmq_str_t s2)
{
    return strcmp(s1, s2) == 0;
}

int tmq_str_startswith(tmq_str_t s, const char* prefix)
{
    size_t len = strlen(s);
    size_t pre_len = strlen(prefix);
    return len < pre_len ? 0 : (strncmp(s, prefix, pre_len) == 0);
}

int tmq_str_is_string(tmq_str_t s)
{
    return (tmq_str_at(s, 0) == '\'' && tmq_str_at(s, tmq_str_len(s) - 1) == '\'') ||
    (tmq_str_at(s, 0) == '\"' && tmq_str_at(s, tmq_str_len(s) - 1) == '\"');
}

tmq_str_t tmq_str_substr(tmq_str_t s, size_t start, size_t len)
{
    if(start >= tmq_str_len(s) || start + len > tmq_str_len(s))
        return NULL;
    tmq_str_t sub = tmq_str_new_len(s + start, len);
    return sub;
}

ssize_t tmq_str_find(tmq_str_t s, char c)
{
    char* pos = strchr(s, c);
    return pos ? pos - s : -1;
}

str_vec tmq_str_split(tmq_str_t s, const char* delimeters)
{
    str_vec parts = tmq_vec_make(tmq_str_t);
    if(!s) return parts;
    tmq_str_t copy = tmq_str_new("");
    copy = tmq_str_assign(copy, s);
    char* p, *saved;
    p = strtok_r(copy, delimeters, &saved);
    while(p)
    {
        tmq_str_t part = tmq_str_new(p);
        tmq_vec_push_back(parts, part);
        p = strtok_r(NULL, delimeters, &saved);
    }
    tmq_str_free(copy);
    return parts;
}

void tmq_str_trim(tmq_str_t s)
{
    size_t len = tmq_str_len(s);
    if(!s || !len) return;
    char* p = s;
    while((*p) && isblank(*p)) p++;
    if(!(*p))
    {
        tmq_str_clear(s);
        return;
    }
    char* p2 = s + len - 1;
    while(isblank(*p2)) p2--;
    size_t new_len = p2 - p + 1;
    memmove(s, p, new_len);
    *(s + new_len) = 0;
    tmq_ds_t* hdr = TMQ_DS_HDR(s);
    hdr->len = new_len;
}