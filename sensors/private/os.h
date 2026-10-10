/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_SENSOR_OS_H
#define QUANTA_SENSOR_OS_H
#include "snapshot_context.h"

typedef struct quanta_os_snapshot
{
    char hostname[QUANTA_TEXT_CAP], kernel[QUANTA_TEXT_CAP];
    char name[QUANTA_TEXT_CAP], version[QUANTA_TEXT_CAP], arch[QUANTA_TEXT_CAP];
    uint64_t online_cpus;
    double uptime, load[3];
    quanta_status hostname_status, kernel_status, uptime_status, load_status, online_status;
} quanta_os_snapshot;

quanta_status quanta_native_os_refresh(quanta_os_snapshot *out, quanta_snapshot_context context);
void quanta_native_os_cleanup(quanta_os_snapshot *data);
#endif
