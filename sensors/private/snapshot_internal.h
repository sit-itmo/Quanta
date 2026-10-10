/**
 * @file sensors/private/snapshot_internal.h
 *
 * Combined private snapshot declarations and native refresh/cleanup functions. This header is
 * private, not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT -- private to native metric implementations */
#ifndef QUANTA_SNAPSHOT_INTERNAL_H
#define QUANTA_SNAPSHOT_INTERNAL_H
#include "snapshot_context.h"
#include "support.h"
#endif
