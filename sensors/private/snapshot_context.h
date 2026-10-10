/**
 * @file sensors/private/snapshot_context.h
 *
 * Collector context: native-data paths and a diagnostic buffer, not a HAL. This header is
 * private, not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT -- private to metric implementations. */
#ifndef QUANTA_SNAPSHOT_CONTEXT_H
#define QUANTA_SNAPSHOT_CONTEXT_H
#include "quanta/types.h"

typedef struct quanta_snapshot_context
{
    quanta_paths paths;
    char *error;
    size_t error_capacity;
} quanta_snapshot_context;
#endif
