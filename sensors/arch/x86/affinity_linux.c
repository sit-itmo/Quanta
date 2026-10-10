/**
 * @file sensors/arch/x86/affinity_linux.c
 *
 * Internal Linux helpers to pin the CPUID collector to a CPU and restore the previous mask.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "affinity.h"
#include <sched.h>
#include <string.h>

int platform_pin_thread(platform_affinity_ctx *ctx)
{
    cpu_set_t original, one;
    unsigned cpu;

    quanta_memset(ctx, 0, sizeof(*ctx));
    if (sched_getaffinity(0, sizeof(original), &original) != 0)
    {
        return -1;
    }
    quanta_memcpy(ctx->original, &original, sizeof(original));

    for (cpu = 0; cpu < CPU_SETSIZE; cpu++)
    {
        if (CPU_ISSET(cpu, &original))
        {
            break;
        }
    }
    if (cpu == CPU_SETSIZE)
    {
        return -1;
    }

    CPU_ZERO(&one);
    CPU_SET(cpu, &one);
    if (sched_setaffinity(0, sizeof(one), &one) != 0)
    {
        return -1;
    }

    ctx->pinned_cpu = cpu;
    ctx->is_pinned = 1;
    return 0;
}

void platform_restore_thread(platform_affinity_ctx *ctx)
{
    cpu_set_t original;
    if (!ctx->is_pinned)
    {
        return;
    }
    quanta_memcpy(&original, ctx->original, sizeof(original));
    sched_setaffinity(0, sizeof(original), &original);
    ctx->is_pinned = 0;
}
