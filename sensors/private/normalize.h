/* SPDX-License-Identifier: MIT -- OS-independent arithmetic and unit normalization */
#ifndef QUANTA_NORMALIZE_H
#define QUANTA_NORMALIZE_H
#include "quanta/types.h"
#ifdef __cplusplus
extern "C"
{
#endif
quanta_status quanta_memory_used(uint64_t total, uint64_t available, uint64_t *used,
                                     double *fraction);
quanta_status quanta_cpu_busy_fraction(uint64_t previous_total, uint64_t previous_idle,
                                      uint64_t total, uint64_t idle, double *fraction);
quanta_status quanta_pages_bytes(uint64_t pages, uint64_t page_size, uint64_t *bytes);
quanta_status quanta_kelvin_celsius(double kelvin, double *celsius);
#ifdef __cplusplus
}
#endif
#endif
