/* SPDX-License-Identifier: MIT */
#include "cpu.h"
#include "snapshot_internal.h"
#include <ctype.h>
#include <unistd.h>

static quanta_status parse_cpu_row(char *line, quanta_cpu_row *row)
{
    char *cursor = line;
    size_t length = 0, fields = 0;
    memset(row, 0, sizeof(*row));
    while (cursor[length] && !isspace((unsigned char)cursor[length]))
    {
        length++;
    }
    if (length < 3 || length >= sizeof(row->name))
    {
        return QUANTA_INVALID_DATA;
    }
    memcpy(row->name, cursor, length);
    for (size_t i = 3; i < length; i++)
    {
        if (!isdigit((unsigned char)row->name[i]))
        {
            return QUANTA_INVALID_DATA;
        }
    }
    cursor += length;
    while (*cursor)
    {
        char token[32];
        size_t n = 0;
        uint64_t value;
        while (isspace((unsigned char)*cursor))
        {
            cursor++;
        }
        if (!*cursor)
        {
            break;
        }
        while (cursor[n] && !isspace((unsigned char)cursor[n]))
        {
            n++;
        }
        if (!n || n >= sizeof(token))
        {
            return QUANTA_INVALID_DATA;
        }
        memcpy(token, cursor, n);
        token[n] = '\0';
        if (!quanta_parse_u64(token, &value))
        {
            return QUANTA_INVALID_DATA;
        }
        /* guest and guest_nice are already included in user/nice: do not double
         * count. */
        if (fields < 8)
        {
            row->t[fields] = value;
        }
        fields++;
        cursor += n;
    }
    return fields >= 4 ? QUANTA_OK : QUANTA_INVALID_DATA;
}

quanta_status quanta_native_cpu_refresh(quanta_cpu_snapshot *out, quanta_snapshot_context context)
{
    const quanta_snapshot_context *ctx = &context;
    char path[QUANTA_PATH_CAP], line[2048];
    FILE *file;
    quanta_status status = QUANTA_OK;
    long frequency = sysconf(_SC_CLK_TCK);
    if (frequency <= 0)
    {
        return QUANTA_IO_ERROR;
    }
    if (!quanta_path(path, sizeof(path), ctx->paths.proc_root, "stat"))
    {
        return QUANTA_LIMIT;
    }
    file = quanta_fopen_utf8(path, "r");
    if (!file)
    {
        return quanta_errno_status(errno);
    }
    out->count = 0;
    out->tick_frequency_hz = (uint64_t)frequency;
    while (fgets(line, sizeof(line), file))
    {
        quanta_cpu_row row;
        if (strncmp(line, "cpu", 3))
        {
            continue;
        }
        if (!strchr(line, '\n') && !feof(file))
        {
            status = QUANTA_LIMIT;
            break;
        }
        status = parse_cpu_row(line, &row);
        if (status != QUANTA_OK)
        {
            break;
        }
        for (size_t i = 0; i < out->count; i++)
        {
            if (!strcmp(row.name, out->rows[i].name))
            {
                status = QUANTA_INVALID_DATA;
                break;
            }
        }
        if (status != QUANTA_OK)
        {
            break;
        }
        if (out->count == out->cap)
        {
            size_t capacity = out->cap ? out->cap * 2 : 32;
            quanta_cpu_row *rows;
            if (out->count >= QUANTA_MAX_CHANNELS)
            {
                status = QUANTA_LIMIT;
                break;
            }
            if (capacity > QUANTA_MAX_CHANNELS)
            {
                capacity = QUANTA_MAX_CHANNELS;
            }
            rows = (quanta_cpu_row *)realloc(out->rows, capacity * sizeof(*rows));
            if (!rows)
            {
                status = QUANTA_NO_MEMORY;
                break;
            }
            out->rows = rows;
            out->cap = capacity;
        }
        out->rows[out->count++] = row;
    }
    if (ferror(file) && status == QUANTA_OK)
    {
        status = QUANTA_IO_ERROR;
    }
    if (fclose(file) && status == QUANTA_OK)
    {
        status = QUANTA_IO_ERROR;
    }
    if (status == QUANTA_OK && (!out->count || strcmp(out->rows[0].name, "cpu")))
    {
        status = QUANTA_NO_DATA;
    }
    out->time_ns = quanta_now_ns();
    return status;
}

void quanta_native_cpu_cleanup(quanta_cpu_snapshot *data)
{
    if (!data)
    {
        return;
    }
    free(data->rows);
    data->rows = NULL;
    data->count = data->cap = 0;
}
