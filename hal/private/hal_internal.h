/**
 * @file hal/private/hal_internal.h
 *
 * Private HAL operation table and implementation state. Unsupported operations return explicit
 * statuses.
 *
 * HAL: hardware primitives and information about their availability. Metrics and SMBIOS/ACPI
 * parsing are not HAL operations.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_HAL_INTERNAL_H
#define QUANTA_HAL_INTERNAL_H
#include "quanta/hal.h"
#include "quanta_config.h"
#include <stdlib.h>
#include <string.h>
quanta_status quanta_hal_platform_open(const quanta_hal_options *, quanta_hal **);
#endif
