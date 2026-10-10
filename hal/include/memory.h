// hal/include/memory.h
#ifndef QUANTA_HAL_MEMORY_H
#define QUANTA_HAL_MEMORY_H
#include "hal.h"

/* Сырой снапшот памяти.
 * Linux: один разбор /proc/meminfo (байты + маска found).
 * Windows: GlobalMemoryStatusEx (физическая) + GetPerformanceInfo
 * (системные commit/object-счётчики, уже в байтах). */
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

quanta_status quanta_hal_parse_meminfo(const char *, quanta_linux_meminfo *);

quanta_status quanta_hal_memory_refresh(quanta_memory_snapshot *out, quanta_hal_context context);
void quanta_hal_memory_cleanup(quanta_memory_snapshot *d);
#endif
