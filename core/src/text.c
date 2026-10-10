/* SPDX-License-Identifier: MIT */
#include "core_internal.h"
#include <ctype.h>
#include <math.h>
#include <time.h>

int quanta_copy(char *d, size_t cap, const char *s)
{
    size_t n = strlen(s);
    if (!cap)
    {
        return 0;
    }
    if (n >= cap)
    {
        d[0] = '\0';
        return 0;
    }
    memcpy(d, s, n + 1);
    return 1;
}

void quanta_trim(char *s)
{
    size_t n = strlen(s), start = 0;
    while (n && isspace((unsigned char)s[n - 1]))
    {
        s[--n] = 0;
    }
    while (s[start] && isspace((unsigned char)s[start]))
    {
        start++;
    }
    if (start)
    {
        memmove(s, s + start, strlen(s + start) + 1);
    }
}

void quanta_safe_text(char *d, size_t cap, const char *s)
{
    size_t used = 0;
    if (!cap)
    {
        return;
    }
    while (*s && used + 1 < cap)
    {
        unsigned char c = (unsigned char)*s;
        unsigned code = 0, min = 0;
        size_t count = 1, j;
        int valid = 1;
        if (c < 0x80)
        {
            d[used++] = (c < 32 || c == 127) ? ' ' : (char)c;
            s++;
            continue;
        }
        if (c >= 0xc2 && c <= 0xdf)
        {
            count = 2;
            code = c & 31;
            min = 0x80;
        }
        else if (c >= 0xe0 && c <= 0xef)
        {
            count = 3;
            code = c & 15;
            min = 0x800;
        }
        else if (c >= 0xf0 && c <= 0xf4)
        {
            count = 4;
            code = c & 7;
            min = 0x10000;
        }
        else
        {
            valid = 0;
        }
        if (valid)
        {
            for (j = 1; j < count; j++)
            {
                unsigned char next = (unsigned char)s[j];
                if (!next || (next & 0xc0) != 0x80)
                {
                    valid = 0;
                    break;
                }
                code = (code << 6) | (next & 63);
            }
        }
        if (valid && (code < min || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)))
        {
            valid = 0;
        }
        if (!valid)
        {
            d[used++] = '?';
            s++;
            continue;
        }
        if ((code >= 0x80 && code <= 0x9f) || (code >= 0x202a && code <= 0x202e) ||
            (code >= 0x2066 && code <= 0x2069))
        {
            d[used++] = ' ';
            s += count;
            continue;
        }
        if (used + count >= cap)
        {
            break;
        }
        memcpy(d + used, s, count);
        used += count;
        s += count;
    }
    d[used] = 0;
}

const char *quanta_label_get(const quanta_label *l, size_t n, const char *key)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        if (!strcmp(l[i].key, key))
        {
            return l[i].value;
        }
    }
    return NULL;
}

int quanta_label_set(quanta_label *l, size_t *n, const char *k, const char *v)
{
    size_t i;
    for (i = 0; i < *n; i++)
    {
        if (!strcmp(l[i].key, k))
        {
            break;
        }
    }
    if (i >= QUANTA_LABEL_CAP)
    {
        return 0;
    }
    if (!quanta_copy(l[i].key, sizeof(l[i].key), k) ||
        !quanta_copy(l[i].value, sizeof(l[i].value), v))
    {
        return 0;
    }
    if (i == *n)
    {
        (*n)++;
    }
    return 1;
}

int quanta_glob_match(const char *p, const char *s)
{
    const char *star = NULL, *retry = NULL;
    while (*s)
    {
        if (*p == '?' || *p == *s)
        {
            p++;
            s++;
        }
        else if (*p == '*')
        {
            star = p++;
            retry = s;
        }
        else if (star)
        {
            p = star + 1;
            s = ++retry;
        }
        else
        {
            return 0;
        }
    }
    while (*p == '*')
    {
        p++;
    }
    return *p == 0;
}

int quanta_parse_u64(const char *s, uint64_t *out)
{
    unsigned long long v;
    char *e;
    while (isspace((unsigned char)*s))
    {
        s++;
    }
    if (!isdigit((unsigned char)*s))
    {
        return 0;
    }
    errno = 0;
    v = strtoull(s, &e, 10);
    if (errno || e == s)
    {
        return 0;
    }
    while (isspace((unsigned char)*e))
    {
        e++;
    }
    if (*e)
    {
        return 0;
    }
    *out = (uint64_t)v;
    return 1;
}

int quanta_parse_s64(const char *s, int64_t *out)
{
    long long v;
    char *e;
    errno = 0;
    v = strtoll(s, &e, 10);
    if (errno || e == s)
    {
        return 0;
    }
    while (isspace((unsigned char)*e))
    {
        e++;
    }
    if (*e)
    {
        return 0;
    }
    *out = (int64_t)v;
    return 1;
}

void quanta_series_name(char *d, size_t cap, const quanta_series *s)
{
    size_t i, off;
    int x = snprintf(d, cap, "%s", s->key);
    if (x < 0)
    {
        return;
    }
    off = (size_t)x;
    for (i = 0; i < s->label_count && off < cap; i++)
    {
        x = snprintf(d + off, cap - off, "%s%s=%s", i ? "," : "{", s->labels[i].key,
                     s->labels[i].value);
        if (x < 0)
        {
            return;
        }
        off += (size_t)x;
    }
    if (s->label_count && off < cap)
    {
        snprintf(d + off, cap - off, "}");
    }
}
