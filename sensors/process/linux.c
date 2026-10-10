/* SPDX-License-Identifier: MIT */
#include "process.h"
#include "snapshot_internal.h"
#include <ctype.h>
#include <unistd.h>

quanta_status quanta_native_process_refresh(quanta_process_snapshot *out, quanta_snapshot_context context)
{
    const quanta_snapshot_context *hal = &context;
    char path[QUANTA_PATH_CAP], buffer[256], *cursor, *end;
    unsigned long long pages = 0;
    long page_size = sysconf(_SC_PAGESIZE);
    unsigned field;
    memset(out, 0, sizeof(*out));
    out->pid = (uint64_t)getpid();
    out->io_status = out->handle_status = QUANTA_UNSUPPORTED;
    if (!quanta_path(path, sizeof(path), hal->paths.proc_root, "self/statm"))
    {
        return out->memory_status = QUANTA_LIMIT;
    }
    out->memory_status = quanta_read_text(path, buffer, sizeof(buffer));
    if (out->memory_status != QUANTA_OK)
    {
        return QUANTA_OK;
    }
    cursor = buffer;
    for (field = 0; field < 2; ++field)
    {
        while (isspace((unsigned char)*cursor))
        {
            ++cursor;
        }
        if (!isdigit((unsigned char)*cursor))
        {
            out->memory_status = QUANTA_INVALID_DATA;
            return QUANTA_OK;
        }
        errno = 0;
        pages = strtoull(cursor, &end, 10);
        if (errno || end == cursor || (*end && !isspace((unsigned char)*end)))
        {
            out->memory_status = QUANTA_INVALID_DATA;
            return QUANTA_OK;
        }
        cursor = end;
    }
    if (page_size <= 0 || pages > UINT64_MAX / (uint64_t)page_size)
    {
        out->memory_status = QUANTA_INVALID_DATA;
    }
    else
    {
        out->rss_bytes = (uint64_t)pages * (uint64_t)page_size;
    }
    return QUANTA_OK;
}

void quanta_native_process_cleanup(quanta_process_snapshot *data)
{
    (void)data;
}
