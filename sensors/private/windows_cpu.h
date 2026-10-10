/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_WINDOWS_CPU_H
#define QUANTA_WINDOWS_CPU_H
/* 0: aggregate, 1: processor, -1: unrelated/invalid. */
int quanta_win_cpu_instance(const char *name, unsigned *group, unsigned *number);
#endif
