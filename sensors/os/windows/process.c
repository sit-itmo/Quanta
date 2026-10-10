/**
 * @file sensors/os/windows/process.c
 *
 * Windows current-Quanta-process collector: use current-process WinAPI calls and populate
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
#include "process.h"
#include "windows_internal.h"
#include <psapi.h>

quanta_status quanta_native_process_refresh(quanta_process_snapshot *out, quanta_snapshot_context context)
{
    PROCESS_MEMORY_COUNTERS_EX memory;
    IO_COUNTERS io;
    DWORD handles = 0;
    HANDLE process = GetCurrentProcess();
    (void)context;
    quanta_memset(out, 0, sizeof(*out));
    quanta_memset(&memory, 0, sizeof(memory));
    quanta_memset(&io, 0, sizeof(io));
    memory.cb = sizeof(memory);
    out->pid = GetCurrentProcessId();
    if (GetProcessMemoryInfo(process, (PROCESS_MEMORY_COUNTERS *)&memory, sizeof(memory)))
    {
        out->rss_bytes = (uint64_t)memory.WorkingSetSize;
        out->private_bytes = (uint64_t)memory.PrivateUsage;
    }
    else
    {
        out->memory_status = quanta_win_status(GetLastError());
    }
    if (GetProcessHandleCount(process, &handles))
    {
        out->handles = handles;
    }
    else
    {
        out->handle_status = quanta_win_status(GetLastError());
    }
    if (GetProcessIoCounters(process, &io))
    {
        out->read_bytes = io.ReadTransferCount;
        out->write_bytes = io.WriteTransferCount;
    }
    else
    {
        out->io_status = quanta_win_status(GetLastError());
    }
    return QUANTA_OK;
}

void quanta_native_process_cleanup(quanta_process_snapshot *data)
{
    (void)data;
}
