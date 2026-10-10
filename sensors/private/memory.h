/**
 * @file sensors/private/memory.h
 *
 * Linux/Windows memory-snapshot fields with independent statuses. This header is private, not
 * public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
#ifndef QUANTA_SENSOR_MEMORY_H
#define QUANTA_SENSOR_MEMORY_H
#include "quanta/osal.h"
#include "snapshot_context.h"

/* Raw memory snapshot. Linux: one /proc/meminfo parse (bytes and a found-field mask). Windows:
 * GlobalMemoryStatusEx for physical memory, plus GetPerformanceInfo for system commit/object
 * counters already converted to bytes. */
typedef struct quanta_memory_snapshot
{
    uint64_t total, available, free, swap_total, swap_free;
    unsigned found;

    uint64_t process_count, thread_count, handle_count;
    uint64_t commit_used, commit_limit, commit_peak;
    uint64_t cache, kernel_paged, kernel_nonpaged;

    quanta_status memory_status, performance_status;
} quanta_memory_snapshot;

typedef struct quanta_linux_meminfo
{
    uint64_t total, available, free, swap_total, swap_free;
    unsigned found;
} quanta_linux_meminfo;

quanta_status quanta_native_parse_meminfo(const char *, quanta_linux_meminfo *);

quanta_status quanta_native_memory_refresh(quanta_memory_snapshot *out, quanta_snapshot_context context);
void quanta_native_memory_cleanup(quanta_memory_snapshot *d);
#endif
