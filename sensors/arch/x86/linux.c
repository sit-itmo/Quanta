// hal/linux/cpuid.c
#include "quanta/osal.h"
#include "affinity.h"
#include "cpuid_snapshot.h"
#include "snapshot_internal.h"
#if QUANTA_WITH_CPUID && QUANTA_ARCH_X86
#include <cpuid.h>

quanta_status quanta_native_cpuid_probe(quanta_cpuid_snapshot *out, quanta_snapshot_context context)
{
    platform_affinity_ctx pin;
    unsigned a, b, c, d, i;
    (void)context;
    quanta_memset(out, 0, sizeof(*out));
    out->status = QUANTA_UNSUPPORTED;
    if (platform_pin_thread(&pin))
    {
        out->status = quanta_errno_status(quanta_errno);
        return QUANTA_OK;
    }
    out->ref = pin.pinned_cpu;
    if (__get_cpuid(0, &a, &b, &c, &d))
    {
        quanta_memcpy(out->vendor, &b, 4);
        quanta_memcpy(out->vendor + 4, &d, 4);
        quanta_memcpy(out->vendor + 8, &c, 4);
        out->status = QUANTA_OK;
        if (__get_cpuid(1, &a, &b, &c, &d))
        {
            out->ecx1 = c;
            out->edx1 = d;
        }
        if (__get_cpuid_count(7, 0, &a, &b, &c, &d))
        {
            out->ebx7 = b;
        }
        if (__get_cpuid(0x80000001u, &a, &b, &c, &d))
        {
            out->ecxext = c;
        }
        if (__get_cpuid_max(0x80000000u, NULL) >= 0x80000004u)
        {
            for (i = 0; i < 3; i++)
            {
                __get_cpuid(0x80000002u + i, &a, &b, &c, &d);
                quanta_memcpy(out->brand + i * 16, &a, 4);
                quanta_memcpy(out->brand + i * 16 + 4, &b, 4);
                quanta_memcpy(out->brand + i * 16 + 8, &c, 4);
                quanta_memcpy(out->brand + i * 16 + 12, &d, 4);
            }
            quanta_trim(out->brand);
        }
    }
    platform_restore_thread(&pin);
    return QUANTA_OK;
}

void quanta_native_cpuid_cleanup(quanta_cpuid_snapshot *d)
{
    (void)d;
}
#endif /* QUANTA_WITH_CPUID && QUANTA_ARCH_X86 */
