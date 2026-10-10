/**
 * @file sensors/private/windows_cpu.h
 *
 * Parse Windows PDH CPU names into group/number pairs. This header is private, not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_WINDOWS_CPU_H
#define QUANTA_WINDOWS_CPU_H
/* 0: aggregate, 1: processor, -1: unrelated/invalid. */
int quanta_win_cpu_instance(const char *name, unsigned *group, unsigned *number);
#endif
