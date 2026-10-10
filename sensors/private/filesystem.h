/**
 * @file sensors/private/filesystem.h
 *
 * Filesystem total/free/available-space snapshot. This header is private, not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
#ifndef QUANTA_SENSOR_FS_H
#define QUANTA_SENSOR_FS_H
#include "quanta/osal.h"
#include "snapshot_context.h"

typedef struct quanta_filesystem_snapshot
{
    uint64_t total, free, available;
    quanta_status status;
} quanta_filesystem_snapshot;

quanta_status quanta_native_fs_refresh(quanta_filesystem_snapshot *out, quanta_snapshot_context context);
void quanta_native_fs_cleanup(quanta_filesystem_snapshot *d);
#endif
