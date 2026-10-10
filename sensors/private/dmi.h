/**
 * @file sensors/private/dmi.h
 *
 * Board collector state and ownership of SMBIOS data. This header is private, not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
#ifndef QUANTA_SENSOR_DMI_H
#define QUANTA_SENSOR_DMI_H

#include "snapshot_context.h"
#include "smbios.h"

typedef struct quanta_board_snapshot
{
    quanta_smbios table;
    quanta_status status;
} quanta_board_snapshot;

quanta_status quanta_native_dmi_refresh(quanta_board_snapshot *out, quanta_snapshot_context context);
void quanta_native_dmi_cleanup(quanta_board_snapshot *d);

#endif
