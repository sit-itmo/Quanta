// hal/include/cpuid.h
#ifndef QUANTA_SENSOR_CPUID_H
#define QUANTA_SENSOR_CPUID_H
#include "snapshot_context.h"

/* CPUID с одного reference CPU.
 * ref: Linux — номер logical CPU; Windows — (group << 16) | processor_number.
 */
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
