/**
 * @file sensors/os/windows/board.c
 *
 * Windows BIOS/board/SMBIOS collector: use the native firmware-table API and populate shared-
 * snapshot data and statuses.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
// hal/windows/dmi.c
#include "quanta/osal.h"
#include "dmi.h"
#include "windows_internal.h"

quanta_status quanta_native_dmi_refresh(quanta_board_snapshot *out, quanta_snapshot_context context)
{
    const quanta_snapshot_context *c = &context;
    UINT size = 0, got = 0;
    unsigned retry;
    const DWORD signature = UINT32_C(0x52534d42); /* 'RSMB' */
    uint8_t *bytes = NULL;

    (void)c;
    out->status = QUANTA_NO_DATA;
    quanta_memset(&out->table, 0, sizeof(out->table));

    for (retry = 0; retry < 3; retry++)
    {
        size = GetSystemFirmwareTable(signature, 0, NULL, 0);
        if (!size)
        {
            DWORD err = GetLastError();
            out->status = err ? quanta_win_status(err) : QUANTA_NO_DATA;
            break;
        }
        if (size > 16u * 1024u * 1024u)
        {
            out->status = QUANTA_LIMIT;
            break;
        }
        bytes = (uint8_t *)quanta_malloc(size);
        if (!bytes)
        {
            out->status = QUANTA_NO_MEMORY;
            break;
        }
        got = GetSystemFirmwareTable(signature, 0, bytes, size);
        if (!got)
        {
            out->status = quanta_win_status(GetLastError());
            if (out->status == QUANTA_OK)
            {
                out->status = QUANTA_NO_DATA;
            }
        }
        else if (got <= size)
        {
            out->status = quanta_smbios_parse_windows(bytes, got, &out->table);
        }
        quanta_free(bytes);
        bytes = NULL;
        if (got <= size)
        {
            break;
        }
        out->status = QUANTA_LIMIT;
    }
    return QUANTA_OK;
}

void quanta_native_dmi_cleanup(quanta_board_snapshot *d)
{
    (void)d;
}
