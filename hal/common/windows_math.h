/* SPDX-License-Identifier: MIT -- OS-independent Windows data normalization */
#ifndef QUANTA_WINDOWS_MATH_H
#define QUANTA_WINDOWS_MATH_H
#include "quanta/types.h"
#ifdef __cplusplus
extern "C"
{
#endif
quanta_status quanta_win_memory_used(uint64_t total, uint64_t available, uint64_t *used,
                                     double *fraction);
quanta_status quanta_win_cpu_fraction(uint64_t previous_total, uint64_t previous_idle,
                                      uint64_t total, uint64_t idle, double *fraction);
quanta_status quanta_win_pages_bytes(uint64_t pages, uint64_t page_size, uint64_t *bytes);
quanta_status quanta_win_kelvin_celsius(double kelvin, double *celsius);
/* 0: aggregate, 1: processor, -1: unrelated/invalid. */
int quanta_win_cpu_instance(const char *name, unsigned *group, unsigned *number);
#ifdef __cplusplus
}
#endif
#endif
