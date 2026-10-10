/* SPDX-License-Identifier: MIT */
#include "affinity.h"
#include <string.h>
#include <windows.h>

int platform_pin_thread(platform_affinity_ctx *ctx)
{
    GROUP_AFFINITY original, one;
    DWORD_PTR mask;
    unsigned bit = 0;

    memset(ctx, 0, sizeof(*ctx));
    if (!GetThreadGroupAffinity(GetCurrentThread(), &original))
    {
        return -1;
    }
    memcpy(ctx->original, &original, sizeof(original));

    mask = original.Mask;
    while (bit < sizeof(mask) * 8 && !(mask & ((DWORD_PTR)1 << bit)))
    {
        bit++;
    }
    if (bit == sizeof(mask) * 8)
    {
        return -1;
    }

    memset(&one, 0, sizeof(one));
    one.Group = original.Group;
    one.Mask = (DWORD_PTR)1 << bit;
    if (!SetThreadGroupAffinity(GetCurrentThread(), &one, NULL))
    {
        return -1;
    }

    ctx->pinned_cpu = ((uint32_t)original.Group << 16) | bit;
    ctx->is_pinned = 1;
    return 0;
}

void platform_restore_thread(platform_affinity_ctx *ctx)
{
    GROUP_AFFINITY original;
    if (!ctx->is_pinned)
    {
        return;
    }
    memcpy(&original, ctx->original, sizeof(original));
    SetThreadGroupAffinity(GetCurrentThread(), &original, NULL);
    ctx->is_pinned = 0;
}
