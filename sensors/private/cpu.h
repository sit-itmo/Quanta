#ifndef QUANTA_SENSOR_CPU_H
#define QUANTA_SENSOR_CPU_H

#include "snapshot_context.h"
#include <stddef.h>
#include <stdint.h>

typedef enum quanta_proc_state
{
    CPU_USER,
    CPU_NICE,
    CPU_SYSTEM,
    CPU_IDLE,
    CPU_IOWAIT,
    CPU_IRQ,
    CPU_SOFTIRQ,
    CPU_STEAL
} quanta_proc_state;

typedef struct quanta_cpu_row
{
    char name[24]; // "cpu", "cpu0", "cpu1"...
    unsigned group, number;
    int direct_utilization;
    double fraction;
    uint64_t window_ns;
    quanta_status utilization_status, accounting_status;
    uint64_t t[8]; // 0:user, 1:nice, 2:system, 3:idle, 4:iowait, 5:irq,
                   // 6:softirq, 7:steal
} quanta_cpu_row;

typedef struct quanta_cpu_snapshot
{
    quanta_cpu_row *rows;
    size_t count, cap;
    uint64_t time_ns;
    uint64_t tick_frequency_hz; // sysconf(_SC_CLK_TCK) on Linux; 10 MHz for Win32
                                // 100 ns ticks
    void *private_data;   /* Opaque platform-owned acquisition state. */
} quanta_cpu_snapshot;

quanta_status quanta_native_cpu_refresh(quanta_cpu_snapshot *out, quanta_snapshot_context context);
void quanta_native_cpu_cleanup(quanta_cpu_snapshot *d);

#endif
