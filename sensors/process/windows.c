/* SPDX-License-Identifier: MIT */
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
    memset(out, 0, sizeof(*out));
    memset(&memory, 0, sizeof(memory));
    memset(&io, 0, sizeof(io));
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
