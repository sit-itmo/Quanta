#ifndef QUANTA_SENSOR_FS_H
#define QUANTA_SENSOR_FS_H
#include "snapshot_context.h"

typedef struct quanta_filesystem_snapshot
{
    uint64_t total, free, available;
    quanta_status status;
} quanta_filesystem_snapshot;

quanta_status quanta_native_fs_refresh(quanta_filesystem_snapshot *out, quanta_snapshot_context context);
void quanta_native_fs_cleanup(quanta_filesystem_snapshot *d);
#endif
