/**
 * @file sensors/private/affinity.h
 *
 * CPU pinning and restoration for the CPUID collector. This header is private, not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

#ifdef _WIN32
#define PLATFORM_AFFINITY_CTX_SIZE 32
#else
#define PLATFORM_AFFINITY_CTX_SIZE 256
#endif

typedef struct
{
    uint8_t original[PLATFORM_AFFINITY_CTX_SIZE];
    uint32_t pinned_cpu;
    int is_pinned;
} platform_affinity_ctx;

int platform_pin_thread(platform_affinity_ctx *ctx);
void platform_restore_thread(platform_affinity_ctx *ctx);
