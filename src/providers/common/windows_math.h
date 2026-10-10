/* SPDX-License-Identifier: MIT -- OS-independent Windows data normalization */
#ifndef INV_WINDOWS_MATH_H
#define INV_WINDOWS_MATH_H
#include "inv/inv.h"
#ifdef __cplusplus
extern "C" {
#endif
inv_status inv_win_memory_used(uint64_t total,uint64_t available,uint64_t *used,double *fraction);
inv_status inv_win_cpu_fraction(uint64_t previous_total,uint64_t previous_idle,
                               uint64_t total,uint64_t idle,double *fraction);
inv_status inv_win_pages_bytes(uint64_t pages,uint64_t page_size,uint64_t *bytes);
inv_status inv_win_kelvin_celsius(double kelvin,double *celsius);
/* 0: aggregate, 1: processor, -1: unrelated/invalid. */
int inv_win_cpu_instance(const char *name,unsigned *group,unsigned *number);
#ifdef __cplusplus
}
#endif
#endif
