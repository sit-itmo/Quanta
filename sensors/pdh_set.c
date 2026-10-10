/* SPDX-License-Identifier: MIT */
#include "pdh_set.h"
#include <math.h>
#define MAX_COUNTERS 4096u
#define MAX_PDH_BUFFER (4u * 1024u * 1024u)

quanta_status quanta_pdh_status(PDH_STATUS status)
{
    if (status == ERROR_SUCCESS || status == PDH_CSTATUS_NEW_DATA)
    {
        return QUANTA_OK;
    }
    if (status == PDH_CSTATUS_NO_OBJECT || status == PDH_CSTATUS_NO_COUNTER)
    {
        return QUANTA_UNSUPPORTED;
    }
    if (status == PDH_CSTATUS_NO_INSTANCE)
    {
        return QUANTA_GONE;
    }
    if (status == PDH_NO_DATA)
    {
        return QUANTA_NO_DATA;
    }
    if (status == PDH_INVALID_DATA)
    {
        return QUANTA_WARMUP;
    }
    if (status == PDH_CSTATUS_INVALID_DATA)
    {
        return QUANTA_INVALID_DATA;
    }
#ifdef PDH_ACCESS_DENIED
    if (status == PDH_ACCESS_DENIED)
    {
        return QUANTA_PERMISSION_DENIED;
    }
#endif
#ifdef PDH_CSTATUS_NO_MACHINE
    if (status == PDH_CSTATUS_NO_MACHINE)
    {
        return QUANTA_NOT_PRESENT;
    }
#endif
    if (status == PDH_MEMORY_ALLOCATION_FAILURE)
    {
        return QUANTA_NO_MEMORY;
    }
    if (status == PDH_MORE_DATA)
    {
        return QUANTA_LIMIT;
    }
    return quanta_win_status((DWORD)status);
}

quanta_status quanta_pdh_open(quanta_pdh_set *s, const wchar_t *path, int delta)
{
    PDH_STATUS status;
    memset(s, 0, sizeof(*s));
    s->english_path = path;
    s->delta_counter = delta;
    status = PdhOpenQueryW(NULL, 0, &s->query);
    return quanta_pdh_status(status);
}

static quanta_status instance_name(const wchar_t *path, char *out, size_t cap)
{
    PDH_COUNTER_PATH_ELEMENTS_W *elements;
    DWORD size = 0;
    PDH_STATUS st = PdhParseCounterPathW(path, NULL, &size, 0);
    quanta_status result = QUANTA_OK;
    if (st != PDH_MORE_DATA)
    {
        return quanta_pdh_status(st);
    }
    if (size > MAX_PDH_BUFFER || size < sizeof(*elements))
    {
        return QUANTA_LIMIT;
    }
    elements = (PDH_COUNTER_PATH_ELEMENTS_W *)malloc(size);
    if (!elements)
    {
        return QUANTA_NO_MEMORY;
    }
    st = PdhParseCounterPathW(path, elements, &size, 0);
    if (st != ERROR_SUCCESS)
    {
        result = quanta_pdh_status(st);
    }
    else if (!elements->szInstanceName)
    {
        result = QUANTA_NO_DATA;
    }
    else if (!quanta_win_utf8(out, cap, elements->szInstanceName))
    {
        result = QUANTA_LIMIT;
    }
    else if (elements->dwInstanceIndex && elements->dwInstanceIndex != ((DWORD) ~(DWORD)0))
    {
        size_t n = strlen(out);
        int written = snprintf(out + n, cap - n, "#%lu", (unsigned long)elements->dwInstanceIndex);
        if (written < 0 || (size_t)written >= cap - n)
        {
            result = QUANTA_LIMIT;
        }
    }
    free(elements);
    return result;
}

static quanta_status add_path(quanta_pdh_set *s, const wchar_t *path)
{
    size_t i, chars;
    quanta_pdh_item *item;
    quanta_status status;
    PDH_STATUS st;
    for (i = 0; i < s->count; i++)
    {
        if (!wcscmp(s->items[i].path, path))
        {
            s->items[i].seen = 1;
            return QUANTA_OK;
        }
    }
    if (s->count == MAX_COUNTERS)
    {
        return QUANTA_LIMIT;
    }
    if (s->count == s->capacity)
    {
        size_t capacity = s->capacity ? s->capacity * 2 : 16;
        quanta_pdh_item *items = (quanta_pdh_item *)realloc(s->items, capacity * sizeof(*items));
        if (!items)
        {
            return QUANTA_NO_MEMORY;
        }
        s->items = items;
        s->capacity = capacity;
    }
    item = &s->items[s->count];
    memset(item, 0, sizeof(*item));
    status = instance_name(path, item->name, sizeof(item->name));
    if (status != QUANTA_OK)
    {
        return status;
    }
    chars = wcslen(path) + 1;
    item->path = (wchar_t *)malloc(chars * sizeof(wchar_t));
    if (!item->path)
    {
        return QUANTA_NO_MEMORY;
    }
    memcpy(item->path, path, chars * sizeof(wchar_t));
    st = PdhAddCounterW(s->query, path, 0, &item->counter);
    if (st != ERROR_SUCCESS)
    {
        free(item->path);
        item->path = NULL;
        return quanta_pdh_status(st);
    }
    item->seen = 1;
    item->status = QUANTA_NO_DATA;
    s->count++;
    return QUANTA_OK;
}

quanta_status quanta_pdh_discover(quanta_pdh_set *s)
{
    PDH_HCOUNTER probe = NULL;
    PDH_COUNTER_INFO_W *info = NULL;
    wchar_t *expanded = NULL;
    DWORD size = 0, chars = 0;
    PDH_STATUS st;
    quanta_status result = QUANTA_OK;
    size_t i, offset;
    unsigned retry;
    if (!s->query)
    {
        return QUANTA_UNSUPPORTED;
    }
    /* The Microsoft-documented localization + wildcard expansion sequence.
       This works on non-English Windows without translated counter names. */
    st = PdhAddEnglishCounterW(s->query, s->english_path, 0, &probe);
    if (st != ERROR_SUCCESS)
    {
        return quanta_pdh_status(st);
    }
    st = PdhGetCounterInfoW(probe, FALSE, &size, NULL);
    if (st != PDH_MORE_DATA)
    {
        result = quanta_pdh_status(st);
        goto done;
    }
    if (size < sizeof(*info) || size > MAX_PDH_BUFFER)
    {
        result = QUANTA_LIMIT;
        goto done;
    }
    info = (PDH_COUNTER_INFO_W *)malloc(size);
    if (!info)
    {
        result = QUANTA_NO_MEMORY;
        goto done;
    }
    st = PdhGetCounterInfoW(probe, FALSE, &size, info);
    if (st != ERROR_SUCCESS)
    {
        result = quanta_pdh_status(st);
        goto done;
    }
    for (retry = 0; retry < 3; retry++)
    {
        chars = 0;
        st = PdhExpandWildCardPathW(NULL, info->szFullPath, NULL, &chars, PDH_REFRESHCOUNTERS);
        if (st != PDH_MORE_DATA)
        {
            result = quanta_pdh_status(st);
            goto done;
        }
        if (chars < 2 || chars > MAX_PDH_BUFFER / sizeof(wchar_t))
        {
            result = QUANTA_LIMIT;
            goto done;
        }
        expanded = (wchar_t *)calloc(chars, sizeof(wchar_t));
        if (!expanded)
        {
            result = QUANTA_NO_MEMORY;
            goto done;
        }
        st = PdhExpandWildCardPathW(NULL, info->szFullPath, expanded, &chars, PDH_REFRESHCOUNTERS);
        if (st != PDH_MORE_DATA)
        {
            break;
        }
        free(expanded);
        expanded = NULL;
    }
    if (st != ERROR_SUCCESS)
    {
        result = quanta_pdh_status(st);
        goto done;
    }
    if (!expanded)
    {
        result = QUANTA_NO_DATA;
        goto done;
    }
    for (i = 0; i < s->count; i++)
    {
        s->items[i].seen = 0;
    }
    offset = 0;
    while (offset < chars && expanded[offset])
    {
        const wchar_t *end = wmemchr(expanded + offset, 0, (size_t)chars - offset);
        if (!end)
        {
            result = QUANTA_INVALID_DATA;
            goto done;
        }
        result = add_path(s, expanded + offset);
        if (result != QUANTA_OK)
        {
            goto done;
        }
        offset = (size_t)(end - expanded) + 1;
    }
    /* Remove only after a complete enumeration, preserving live baselines. */
    for (i = 0; i < s->count;)
    {
        if (s->items[i].seen)
        {
            i++;
            continue;
        }
        PdhRemoveCounter(s->items[i].counter);
        free(s->items[i].path);
        memmove(s->items + i, s->items + i + 1, (s->count - i - 1) * sizeof(*s->items));
        s->count--;
    }
done:
    if (probe)
    {
        PdhRemoveCounter(probe);
    }
    free(info);
    free(expanded);
    return result;
}

quanta_status quanta_pdh_collect(quanta_pdh_set *s)
{
    PDH_STATUS st;
    uint64_t now;
    size_t i;
    if (!s->query)
    {
        return QUANTA_UNSUPPORTED;
    }
    if (!s->count)
    {
        return QUANTA_NO_DATA;
    }
    st = PdhCollectQueryData(s->query);
    now = quanta_now_ns();
    for (i = 0; i < s->count; i++)
    {
        quanta_pdh_item *item = &s->items[i];
        PDH_FMT_COUNTERVALUE value;
        PDH_STATUS status = st;
        item->window_ns =
            item->collected_ns && now > item->collected_ns ? now - item->collected_ns : 0;
        memset(&value, 0, sizeof(value));
        if (status == ERROR_SUCCESS)
        {
            status = PdhGetFormattedCounterValue(
                item->counter, PDH_FMT_DOUBLE | PDH_FMT_NOSCALE | PDH_FMT_NOCAP100, NULL, &value);
        }
        item->status = quanta_pdh_status(status);
        if (item->status == QUANTA_OK)
        {
            item->status = quanta_pdh_status((PDH_STATUS)value.CStatus);
        }
        if (st == ERROR_SUCCESS && !item->samples && s->delta_counter)
        {
            item->status = QUANTA_WARMUP;
        }
        if (item->status == QUANTA_OK)
        {
            item->value = value.doubleValue;
            if (!isfinite(item->value))
            {
                item->status = QUANTA_INVALID_DATA;
            }
        }
        if (st == ERROR_SUCCESS)
        {
            if (item->samples < UINT_MAX)
            {
                item->samples++;
            }
            item->collected_ns = now;
        }
        else
        {
            item->samples = 0;
            item->collected_ns = 0;
        }
    }
    return quanta_pdh_status(st);
}

const quanta_pdh_item *quanta_pdh_find(const quanta_pdh_set *s, const char *name)
{
    size_t i;
    for (i = 0; i < s->count; i++)
    {
        if (!strcmp(s->items[i].name, name))
        {
            return &s->items[i];
        }
    }
    return NULL;
}

void quanta_pdh_close(quanta_pdh_set *s)
{
    size_t i;
    if (s->query)
    {
        PdhCloseQuery(s->query);
    }
    for (i = 0; i < s->count; i++)
    {
        free(s->items[i].path);
    }
    free(s->items);
    memset(s, 0, sizeof(*s));
}
