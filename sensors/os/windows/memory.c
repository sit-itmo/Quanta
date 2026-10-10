/**
 * @file sensors/os/windows/memory.c
 *
 * Windows memory collector: use GlobalMemoryStatusEx/GetPerformanceInfo and populate shared-
 * snapshot data and statuses.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
// hal/windows/memory.c
#include "quanta/osal.h"
#include "memory.h"
#include "windows_internal.h"
#include "normalize.h"
#include "windows_cpu.h"
#include <psapi.h>

quanta_status quanta_native_memory_refresh(quanta_memory_snapshot *out, quanta_snapshot_context context)
{
    const quanta_snapshot_context *c = &context;
    MEMORYSTATUSEX memory;
    PERFORMANCE_INFORMATION performance;
    quanta_memset(&memory, 0, sizeof(memory));
    memory.dwLength = sizeof(memory);
    quanta_memset(&performance, 0, sizeof(performance));
    performance.cb = sizeof(performance);

    out->found = 0;
    if (!GlobalMemoryStatusEx(&memory))
    {
        DWORD error = GetLastError();
        quanta_win_error(c->error, c->error_capacity, error, "GlobalMemoryStatusEx");
        out->memory_status = quanta_win_status(error);
    }
    else
    {
        out->memory_status = QUANTA_OK;
        out->total = memory.ullTotalPhys;
        out->available = memory.ullAvailPhys;
        out->found |= 3u;
    }

    if (!GetPerformanceInfo(&performance, sizeof(performance)))
    {
        DWORD error = GetLastError();
        quanta_win_error(c->error, c->error_capacity, error, "GetPerformanceInfo");
        out->performance_status = quanta_win_status(error);
    }
    else
    {
        out->performance_status = QUANTA_OK;
        out->process_count = performance.ProcessCount;
        out->thread_count = performance.ThreadCount;
        out->handle_count = performance.HandleCount;
        /* Checked page-to-byte conversion, once per shared snapshot. */
        if (quanta_pages_bytes(performance.CommitTotal, performance.PageSize,
                                   &out->commit_used) != QUANTA_OK ||
            quanta_pages_bytes(performance.CommitLimit, performance.PageSize,
                                   &out->commit_limit) != QUANTA_OK ||
            quanta_pages_bytes(performance.CommitPeak, performance.PageSize,
                                   &out->commit_peak) != QUANTA_OK ||
            quanta_pages_bytes(performance.SystemCache, performance.PageSize, &out->cache) !=
                QUANTA_OK ||
            quanta_pages_bytes(performance.KernelPaged, performance.PageSize,
                                   &out->kernel_paged) != QUANTA_OK ||
            quanta_pages_bytes(performance.KernelNonpaged, performance.PageSize,
                                   &out->kernel_nonpaged) != QUANTA_OK)
        {
            out->performance_status = QUANTA_INVALID_DATA;
        }
    }
    return QUANTA_OK; /* Independent source failures remain per-field. */
}

void quanta_native_memory_cleanup(quanta_memory_snapshot *d)
{
    (void)d;
}
