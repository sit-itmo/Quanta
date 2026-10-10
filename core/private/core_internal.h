/**
 * @file core/private/core_internal.h
 *
 * Private core declarations for query matching and metric validation; not part of the public
 * API.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 *
 * Plan: a parsed query containing selected metrics, polling intervals, history, HAL and
 * presentation settings. Parsing does not access hardware.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_CORE_INTERNAL_H
#define QUANTA_CORE_INTERNAL_H
#include "quanta/quanta.h"
#include "support.h"
#include "quanta/osal.h"
#include "quanta/hal.h"
#ifndef QUANTA_INITIAL_CAPACITY
#define QUANTA_INITIAL_CAPACITY 32u
#endif
#ifndef QUANTA_MAX_ACQUISITIONS
#define QUANTA_MAX_ACQUISITIONS 32u
#endif
#endif
