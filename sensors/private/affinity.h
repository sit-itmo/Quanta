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
