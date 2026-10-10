/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_HAL_PROCESS_H
#define QUANTA_HAL_PROCESS_H
#include "hal.h"

typedef struct quanta_process_snapshot
{
    uint64_t rss_bytes, private_bytes, pid, handles, read_bytes, write_bytes;
    quanta_status memory_status, handle_status, io_status;
} quanta_process_snapshot;

quanta_status quanta_hal_process_refresh(quanta_process_snapshot *out, quanta_hal_context context);
void quanta_hal_process_cleanup(quanta_process_snapshot *data);
#endif
