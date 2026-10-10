#ifndef QUANTA_HAL_FS_H
#define QUANTA_HAL_FS_H
#include "hal.h"

typedef struct quanta_filesystem_snapshot
{
    uint64_t total, free, available;
    quanta_status status;
} quanta_filesystem_snapshot;

quanta_status quanta_hal_fs_refresh(quanta_filesystem_snapshot *out, quanta_hal_context context);
void quanta_hal_fs_cleanup(quanta_filesystem_snapshot *d);
#endif
