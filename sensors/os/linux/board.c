/**
 * @file sensors/os/linux/board.c
 *
 * Linux BIOS/board/SMBIOS collector: read SMBIOS through system files and populate shared-
 * snapshot data and statuses.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
// hal/linux/dmi.c
#include "quanta/osal.h"
#include "dmi.h"
#include "snapshot_internal.h"

quanta_status quanta_native_dmi_refresh(quanta_board_snapshot *out, quanta_snapshot_context context)
{
    const quanta_snapshot_context *c = &context;
    char rel[256], p[QUANTA_PATH_CAP];
    quanta_status s;
    size_t i;

    quanta_memset(&out->table, 0, sizeof(out->table));

    static const char *const fields[] = {"bios_vendor",  "bios_version", "bios_date",
                                         "board_vendor", "board_name",   "board_serial",
                                         "sys_vendor",   "product_name", "product_serial"};
    static const int field_idx[] = {
        QUANTA_BIOS_VENDOR,   QUANTA_BIOS_VERSION, QUANTA_BIOS_DATE,
        QUANTA_BOARD_VENDOR,  QUANTA_BOARD_MODEL,  QUANTA_BOARD_SERIAL,
        QUANTA_SYSTEM_VENDOR, QUANTA_SYSTEM_MODEL, QUANTA_SYSTEM_SERIAL};

    for (i = 0; i < QUANTA_ARRAY_SIZE(fields); i++)
    {
        quanta_snprintf(rel, sizeof(rel), "class/dmi/id/%s", fields[i]);
        if (!quanta_path(p, sizeof(p), c->paths.sys_root, rel))
        {
            out->table.fields[field_idx[i]].status = QUANTA_LIMIT;
            continue;
        }
        s = quanta_read_text(p, out->table.fields[field_idx[i]].text,
                             sizeof(out->table.fields[field_idx[i]].text));
        if (s != QUANTA_OK)
        {
            out->table.fields[field_idx[i]].status = s;
            continue;
        }
        quanta_trim(out->table.fields[field_idx[i]].text);
        out->table.fields[field_idx[i]].status =
            out->table.fields[field_idx[i]].text[0] ? QUANTA_OK : QUANTA_NO_DATA;
    }

    /* UUID */
    quanta_snprintf(rel, sizeof(rel), "class/dmi/id/product_uuid");
    if (quanta_path(p, sizeof(p), c->paths.sys_root, rel))
    {
        s = quanta_read_text(p, out->table.uuid.text, sizeof(out->table.uuid.text));
        if (s == QUANTA_OK)
        {
            quanta_trim(out->table.uuid.text);
            out->table.uuid.status = out->table.uuid.text[0] ? QUANTA_OK : QUANTA_NO_DATA;
        }
        else
        {
            out->table.uuid.status = s;
        }
    }
    else
    {
        out->table.uuid.status = QUANTA_LIMIT;
    }

    out->status = QUANTA_OK;
    return QUANTA_OK;
}

void quanta_native_dmi_cleanup(quanta_board_snapshot *d)
{
    (void)d;
}
