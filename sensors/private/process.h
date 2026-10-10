/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_SENSOR_PROCESS_H
#define QUANTA_SENSOR_PROCESS_H
#include "snapshot_context.h"

typedef struct quanta_process_snapshot
{
    uint64_t rss_bytes, private_bytes, pid, handles, read_bytes, write_bytes;
    quanta_status memory_status, handle_status, io_status;
} quanta_process_snapshot;

quanta_status quanta_native_process_refresh(quanta_process_snapshot *out, quanta_snapshot_context context);
void quanta_native_process_cleanup(quanta_process_snapshot *data);
#endif
