/**
 * @file sensors/os/linux/network.c
 *
 * Linux network-interface collector: read /proc/net/dev and sysfs attributes, then populate
 * shared-snapshot data and statuses.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "network.h"
#include "snapshot_internal.h"
#include <ctype.h>
#include <dirent.h>

static quanta_status add_row(quanta_network_snapshot *out, const quanta_network_row *row)
{
    if (out->count >= QUANTA_MAX_CHANNELS)
    {
        return QUANTA_LIMIT;
    }
    if (out->count == out->cap)
    {
        size_t capacity = out->cap ? out->cap * 2 : 16;
        quanta_network_row *rows;
        if (capacity > QUANTA_MAX_CHANNELS)
        {
            capacity = QUANTA_MAX_CHANNELS;
        }
        rows = quanta_realloc(out->rows, capacity * sizeof(*rows));
        if (!rows)
        {
            return QUANTA_NO_MEMORY;
        }
        out->rows = rows;
        out->cap = capacity;
    }
    out->rows[out->count++] = *row;
    return QUANTA_OK;
}

static quanta_status read_attribute(const char *base, const char *name, char *out, size_t capacity)
{
    char path[QUANTA_PATH_CAP];
    quanta_status status;
    if (!quanta_path(path, sizeof(path), base, name))
    {
        return QUANTA_LIMIT;
    }
    status = quanta_read_text(path, out, capacity);
    if (status == QUANTA_OK)
    {
        quanta_trim(out);
    }
    return status;
}

static quanta_status apply_line(const char *line, quanta_network_snapshot *out)
{
    const char *cursor = line, *colon;
    uint64_t values[16];
    size_t i, k;
    while (quanta_isspace((unsigned char)*cursor))
    {
        ++cursor;
    }
    colon = quanta_strchr(cursor, ':');
    if (!colon)
    {
        return QUANTA_OK;
    }
    for (i = 0; i < out->count; ++i)
    {
        if (quanta_strlen(out->rows[i].name) == (size_t)(colon - cursor) &&
            !quanta_memcmp(out->rows[i].name, cursor, (size_t)(colon - cursor)))
        {
            break;
        }
    }
    if (i == out->count)
    {
        return QUANTA_OK;
    }
    if (out->rows[i].counters_status == QUANTA_OK)
    {
        return QUANTA_INVALID_DATA;
    }
    cursor = colon + 1;
    for (k = 0; k < 16; ++k)
    {
        char *end;
        while (quanta_isspace((unsigned char)*cursor))
        {
            ++cursor;
        }
        if (!quanta_isdigit((unsigned char)*cursor))
        {
            return QUANTA_INVALID_DATA;
        }
        quanta_errno = 0;
        values[k] = quanta_strtoull(cursor, &end, 10);
        if (quanta_errno || end == cursor || (*end && !quanta_isspace((unsigned char)*end)))
        {
            return QUANTA_INVALID_DATA;
        }
        cursor = end;
    }
    while (quanta_isspace((unsigned char)*cursor))
    {
        ++cursor;
    }
    if (*cursor)
    {
        return QUANTA_INVALID_DATA;
    }
    out->rows[i].rx_bytes = values[0];
    out->rows[i].rx_packets = values[1];
    out->rows[i].rx_errors = values[2];
    out->rows[i].rx_dropped = values[3];
    out->rows[i].tx_bytes = values[8];
    out->rows[i].tx_packets = values[9];
    out->rows[i].tx_errors = values[10];
    out->rows[i].tx_dropped = values[11];
    out->rows[i].counters_status = QUANTA_OK;
    return QUANTA_OK;
}

quanta_status quanta_native_network_refresh(quanta_network_snapshot *out, quanta_snapshot_context context)
{
    const quanta_snapshot_context *hal = &context;
    char root[QUANTA_PATH_CAP], base[QUANTA_PATH_CAP], path[QUANTA_PATH_CAP];
    char buffer[QUANTA_TEXT_CAP];
    DIR *directory;
    struct dirent *entry;
    quanta_file *file;
    quanta_status status = QUANTA_OK;
    size_t i;
    out->count = 0;
    if (!quanta_path(root, sizeof(root), hal->paths.sys_root, "class/net"))
    {
        return QUANTA_LIMIT;
    }
    directory = opendir(root);
    if (!directory)
    {
        return out->status = quanta_errno_status(quanta_errno);
    }
    while ((entry = readdir(directory)) != NULL)
    {
        quanta_network_row row;
        if (entry->d_name[0] == '.')
        {
            continue;
        }
        quanta_memset(&row, 0, sizeof(row));
        row.counters_status = QUANTA_NO_DATA;
        row.link_status = QUANTA_UNSUPPORTED;
        if (!quanta_path(base, sizeof(base), root, entry->d_name) ||
            !quanta_copy(row.name, sizeof(row.name), entry->d_name))
        {
            status = QUANTA_LIMIT;
            break;
        }
        status = read_attribute(base, "ifindex", buffer, sizeof(buffer));
        if (status != QUANTA_OK)
        {
            break; /* A failed enumeration must not remove known series. */
        }
        if (!quanta_parse_u64(buffer, &row.ifindex) || !row.ifindex)
        {
            status = QUANTA_INVALID_DATA;
            break;
        }
        quanta_snprintf(row.entity_key, sizeof(row.entity_key), "ifindex:%" PRIu64, row.ifindex);
        row.address_status = read_attribute(base, "address", row.address, sizeof(row.address));
        row.state_status = read_attribute(base, "operstate", row.state, sizeof(row.state));
        status = add_row(out, &row);
        if (status != QUANTA_OK)
        {
            break;
        }
    }
    closedir(directory);
    if (status != QUANTA_OK)
    {
        return out->status = status;
    }
    if (!quanta_path(path, sizeof(path), hal->paths.proc_root, "net/dev"))
    {
        return QUANTA_LIMIT;
    }
    file = quanta_fopen(path, "r");
    if (!file)
    {
        status = quanta_errno_status(quanta_errno);
    }
    else
    {
        char line[2048];
        while (quanta_fgets(line, sizeof(line), file))
        {
            if (!quanta_strchr(line, '\n') && !quanta_feof(file))
            {
                status = QUANTA_LIMIT;
                break;
            }
            status = apply_line(line, out);
            if (status != QUANTA_OK)
            {
                break;
            }
        }
        if (quanta_ferror(file))
        {
            status = QUANTA_IO_ERROR;
        }
        quanta_fclose(file);
    }
    /* Identity is still discoverable if just the counter source is unavailable.
     */
    if (status != QUANTA_OK)
    {
        for (i = 0; i < out->count; ++i)
        {
            out->rows[i].counters_status = status;
        }
    }
    out->status = QUANTA_OK;
    return QUANTA_OK;
}

void quanta_native_network_cleanup(quanta_network_snapshot *data)
{
    if (!data)
    {
        return;
    }
    quanta_free(data->rows);
    data->rows = NULL;
    data->count = data->cap = 0;
}
