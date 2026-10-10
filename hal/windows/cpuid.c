// hal/windows/cpuid.c
#include "affinity.h"
#include "quanta_hal_cpuid.h"
#include "windows_internal.h"
#if QUANTA_WITH_CPUID && QUANTA_ARCH_X86
#include <intrin.h>

quanta_status quanta_hal_cpuid_probe(quanta_cpuid_snapshot *out, quanta_hal_context context)
{
    const quanta_hal_context *c = &context;
    platform_affinity_ctx pin;
    int r[4];
    unsigned max, emax, i;
    memset(out, 0, sizeof(*out));
    out->status = QUANTA_UNSUPPORTED;
    if (platform_pin_thread(&pin))
    {
        DWORD err = GetLastError();
        if (c)
        {
            quanta_win_error(c->error, c->error_capacity, err, "SetThreadGroupAffinity");
        }
        out->status = err ? quanta_win_status(err) : QUANTA_UNSUPPORTED;
        return QUANTA_OK;
    }
    out->ref = pin.pinned_cpu; /* Packed processor group and logical number. */
    __cpuidex(r, 0, 0);
    max = (unsigned)r[0];
    if (max)
    {
        memcpy(out->vendor, &r[1], 4);
        memcpy(out->vendor + 4, &r[3], 4);
        memcpy(out->vendor + 8, &r[2], 4);
        out->status = QUANTA_OK;
        if (max >= 1)
        {
            __cpuidex(r, 1, 0);
            out->ecx1 = (uint32_t)r[2];
            out->edx1 = (uint32_t)r[3];
        }
        if (max >= 7)
        {
            __cpuidex(r, 7, 0);
            out->ebx7 = (uint32_t)r[1];
        }
    }
    __cpuidex(r, 0x80000000u, 0);
    emax = (unsigned)r[0];
    if (emax >= 0x80000001u)
    {
        __cpuidex(r, 0x80000001u, 0);
        out->ecxext = (uint32_t)r[2];
    }
    if (emax >= 0x80000004u)
    {
        for (i = 0; i < 3; i++)
        {
            __cpuidex(r, (int)(0x80000002u + i), 0);
            memcpy(out->brand + i * 16, &r[0], 4);
            memcpy(out->brand + i * 16 + 4, &r[1], 4);
            memcpy(out->brand + i * 16 + 8, &r[2], 4);
            memcpy(out->brand + i * 16 + 12, &r[3], 4);
        }
        quanta_trim(out->brand);
    }
    platform_restore_thread(&pin);
    return QUANTA_OK;
}

void quanta_hal_cpuid_cleanup(quanta_cpuid_snapshot *d)
{
    (void)d;
}
#endif /* QUANTA_WITH_CPUID && QUANTA_ARCH_X86 */
