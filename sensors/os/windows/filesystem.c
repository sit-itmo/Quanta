/**
 * @file sensors/os/windows/filesystem.c
 *
 * Windows filesystem collector: read free/total system-volume space and populate shared-snapshot
 * data and statuses.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
#include "quanta/osal.h"
#include "filesystem.h"
#include "windows_native.h"

quanta_status quanta_native_fs_refresh(quanta_filesystem_snapshot *out, quanta_snapshot_context context)
{
    const quanta_snapshot_context *c = &context;
    wchar_t root[4];
    ULARGE_INTEGER total, available;
    if (!quanta_win_system_drive(root, QUANTA_ARRAY_SIZE(root)))
    {
        out->status = QUANTA_UNSUPPORTED;
        return QUANTA_OK;
    }
    if (!GetDiskFreeSpaceExW(root, &available, &total, NULL))
    {
        DWORD error = GetLastError();
        quanta_win_error(c->error, c->error_capacity, error, "GetDiskFreeSpaceExW");
        out->status = quanta_win_status(error);
        return QUANTA_OK;
    }
    out->total = total.QuadPart;
    out->available = available.QuadPart;
    out->free = available.QuadPart;
    /* Windows quota-aware used space is total minus available. */
    out->status = out->total && out->available <= out->total ? QUANTA_OK : QUANTA_INVALID_DATA;
    return QUANTA_OK;
}

void quanta_native_fs_cleanup(quanta_filesystem_snapshot *d)
{
    (void)d;
}
