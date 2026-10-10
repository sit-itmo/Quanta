/**
 * @file sensors/private/cpuid_snapshot.h
 *
 * CPU architectural-property snapshot, feature flags and CPUID identification strings. This
 * header is private, not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
#ifndef QUANTA_SENSOR_CPUID_H
#define QUANTA_SENSOR_CPUID_H
#include "snapshot_context.h"

/* CPUID collected from one reference CPU. On Linux, ref is the logical CPU number; on Windows,
 * ref is (group << 16) | processor_number. */
typedef struct quanta_cpuid_snapshot
{
    char vendor[16];
    char brand[64];
    unsigned ref;
    uint32_t ecx1, edx1, ebx7, ecxext;
    quanta_status status;
} quanta_cpuid_snapshot;

quanta_status quanta_native_cpuid_probe(quanta_cpuid_snapshot *out, quanta_snapshot_context context);
void quanta_native_cpuid_cleanup(quanta_cpuid_snapshot *d);
#endif
