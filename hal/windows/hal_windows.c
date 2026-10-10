/* SPDX-License-Identifier: MIT -- no driver and no ring-0 emulation. */
#include "hal_internal.h"
#include <windows.h>

static quanta_status win_status(DWORD error)
{
    if (error == ERROR_ACCESS_DENIED || error == ERROR_PRIVILEGE_NOT_HELD) return QUANTA_PERMISSION_DENIED;
    if (error == ERROR_INVALID_PARAMETER) return QUANTA_INVALID_DATA;
    if (error == ERROR_NOT_SUPPORTED || error == ERROR_CALL_NOT_IMPLEMENTED) return QUANTA_UNSUPPORTED;
    return QUANTA_IO_ERROR;
}

static quanta_status set_affinity(void *data, quanta_cpu_address cpu, quanta_affinity_state *old)
{
    GROUP_AFFINITY one, previous;
    (void)data;
    _Static_assert(sizeof(previous) <= sizeof(old->native), "affinity state is too small");
    if (cpu.number >= sizeof(KAFFINITY) * 8 || cpu.group >= GetActiveProcessorGroupCount())
        return QUANTA_INVALID_DATA;
    if (cpu.number >= GetActiveProcessorCount(cpu.group)) return QUANTA_NOT_PRESENT;
    memset(&one, 0, sizeof(one));
    one.Group = cpu.group;
    one.Mask = (KAFFINITY)1 << cpu.number;
    if (!SetThreadGroupAffinity(GetCurrentThread(), &one, &previous)) return win_status(GetLastError());
    memcpy(old->native, &previous, sizeof(previous));
    old->size = sizeof(previous);
    old->owner_thread = GetCurrentThreadId();
    old->active = 1;
    return QUANTA_OK;
}

static quanta_status restore_affinity(void *data, quanta_affinity_state *old)
{
    GROUP_AFFINITY previous;
    (void)data;
    if (old->size != sizeof(previous) || old->owner_thread != GetCurrentThreadId()) return QUANTA_INVALID_DATA;
    memcpy(&previous, old->native, sizeof(previous));
    if (!SetThreadGroupAffinity(GetCurrentThread(), &previous, NULL)) return win_status(GetLastError());
    memset(old, 0, sizeof(*old));
    return QUANTA_OK;
}

quanta_status quanta_hal_platform_open(const quanta_hal_options *options, quanta_hal **out)
{
    quanta_hal_info info = {0};
    quanta_hal_operations ops = {0};
    MEMORYSTATUSEX memory;
    const char *name = options->name;
    if (name && *name && strcmp(name, "native") && strcmp(name, "windows-native")) return QUANTA_UNSUPPORTED;
    strcpy(info.name, "windows-native");
    info.logical_cpus = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (info.logical_cpus) info.known |= QUANTA_HAL_CPU_COUNT_KNOWN;
    memset(&memory, 0, sizeof(memory));
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory))
    {
        info.physical_memory_bytes = memory.ullTotalPhys;
        info.known |= QUANTA_HAL_MEMORY_SIZE_KNOWN;
    }
    ops.set_affinity = set_affinity;
    ops.restore_affinity = restore_affinity;
    return quanta_hal_attach(&info, &ops, NULL, options, out);
}
