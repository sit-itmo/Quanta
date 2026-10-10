/**
 * @file include/quanta/quanta.h
 *
 * Umbrella header for the public Quanta API; consumers use a single include directory.
 *
 * Catalog: an immutable set of metric descriptors with unique names. It neither stores
 * measurement results nor selects providers.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_QUANTA_H
#define QUANTA_QUANTA_H
#include "engine.h"
#include "history.h"
#include "live.h"
#include "metric.h"
#include "query.h"
#include "registry.h"
#include "text.h"
#include "types.h"
#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Aggregate public, portable C API. Native Windows/pthread types stay private.
 * Low-level HAL functions are available separately in quanta/hal.h. */
#ifdef __cplusplus
}
#endif
#endif
