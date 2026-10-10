// hal/linux/memory.c
#include "memory.h"
#include "snapshot_internal.h"
#include <ctype.h>

/* Recognized fields require a complete "<unsigned> kB" value. Unknown fields
 * are skipped within their own line; they cannot consume a later field's colon.
 */
quanta_status quanta_native_parse_meminfo(const char *text, quanta_linux_meminfo *data)
{
    const char *line;
    if (!text || !data)
    {
        return QUANTA_INVALID_DATA;
    }
    memset(data, 0, sizeof(*data));
    for (line = text; *line;)
    {
        const char *end = strchr(line, '\n');
        const char *colon, *cursor;
        uint64_t value = 0, *slot = NULL;
        unsigned bit = 0;
        size_t length;
        if (!end)
        {
            end = line + strlen(line);
        }
        colon = memchr(line, ':', (size_t)(end - line));
        if (!colon)
        {
            line = *end ? end + 1 : end;
            continue;
        }
        length = (size_t)(colon - line);
        if (length == 8 && !memcmp(line, "MemTotal", 8))
        {
            bit = 1;
            slot = &data->total;
        }
        else if (length == 12 && !memcmp(line, "MemAvailable", 12))
        {
            bit = 2;
            slot = &data->available;
        }
        else if (length == 7 && !memcmp(line, "MemFree", 7))
        {
            bit = 4;
            slot = &data->free;
        }
        else if (length == 9 && !memcmp(line, "SwapTotal", 9))
        {
            bit = 8;
            slot = &data->swap_total;
        }
        else if (length == 8 && !memcmp(line, "SwapFree", 8))
        {
            bit = 16;
            slot = &data->swap_free;
        }
        if (!bit)
        {
            line = *end ? end + 1 : end;
            continue;
        }
        if (data->found & bit)
        {
            return QUANTA_INVALID_DATA;
        }
        cursor = colon + 1;
        while (cursor < end && (*cursor == ' ' || *cursor == '\t'))
        {
            ++cursor;
        }
        if (cursor == end || !isdigit((unsigned char)*cursor))
        {
            return QUANTA_INVALID_DATA;
        }
        while (cursor < end && isdigit((unsigned char)*cursor))
        {
            unsigned digit = (unsigned)(*cursor++ - '0');
            if (value > (UINT64_MAX - digit) / 10)
            {
                return QUANTA_INVALID_DATA;
            }
            value = value * 10 + digit;
        }
        if (cursor == end || (*cursor != ' ' && *cursor != '\t'))
        {
            return QUANTA_INVALID_DATA;
        }
        while (cursor < end && (*cursor == ' ' || *cursor == '\t'))
        {
            ++cursor;
        }
        if (end - cursor < 2 || memcmp(cursor, "kB", 2))
        {
            return QUANTA_INVALID_DATA;
        }
        cursor += 2;
        while (cursor < end && isspace((unsigned char)*cursor))
        {
            ++cursor;
        }
        if (cursor != end || value > UINT64_MAX / 1024)
        {
            return QUANTA_INVALID_DATA;
        }
        *slot = value * 1024;
        data->found |= bit;
        line = *end ? end + 1 : end;
    }
    if (!(data->found & 1))
    {
        return QUANTA_NO_DATA;
    }
    if ((data->found & 3) == 3 && data->available > data->total)
    {
        return QUANTA_INVALID_DATA;
    }
    if ((data->found & 5) == 5 && data->free > data->total)
    {
        return QUANTA_INVALID_DATA;
    }
    if ((data->found & 24) == 24 && data->swap_free > data->swap_total)
    {
        return QUANTA_INVALID_DATA;
    }
    return QUANTA_OK;
}

quanta_status quanta_native_memory_refresh(quanta_memory_snapshot *out, quanta_snapshot_context context)
{
    quanta_linux_meminfo memory;
    char path[QUANTA_PATH_CAP], buffer[32768];
    quanta_status status;
    out->performance_status = QUANTA_UNSUPPORTED;
    if (!quanta_path(path, sizeof(path), context.paths.proc_root, "meminfo"))
    {
        out->memory_status = QUANTA_LIMIT;
        return QUANTA_LIMIT;
    }
    status = quanta_read_text(path, buffer, sizeof(buffer));
    if (status == QUANTA_OK)
    {
        status = quanta_native_parse_meminfo(buffer, &memory);
    }
    out->memory_status = status;
    if (status != QUANTA_OK)
    {
        return status;
    }
    out->total = memory.total;
    out->available = memory.available;
    out->free = memory.free;
    out->swap_total = memory.swap_total;
    out->swap_free = memory.swap_free;
    out->found = memory.found;
    return QUANTA_OK;
}

void quanta_native_memory_cleanup(quanta_memory_snapshot *data)
{
    (void)data;
}
