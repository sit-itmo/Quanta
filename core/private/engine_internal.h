/**
 * @file core/private/engine_internal.h
 *
 * Internal engine state: acquisition groups, channels, presentation bindings and polling
 * deadlines.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Series: a particular metric instance with labels and an identity. Replacing a device may start
 * a new series even when its name is unchanged.
 *
 * Plan: a parsed query containing selected metrics, polling intervals, history, HAL and
 * presentation settings. Parsing does not access hardware.
 */
/* SPDX-License-Identifier: MIT -- private scheduler state, never installed */
#ifndef QUANTA_ENGINE_INTERNAL_H
#define QUANTA_ENGINE_INTERNAL_H
#include "core_internal.h"

typedef struct quanta_channel
{
    quanta_instance in;
    size_t acquisition_index;
    quanta_sample last;
    uint64_t period, next;
    uint64_t seen;
    int active, updated, initialized;
} quanta_channel;

typedef struct quanta_binding
{
    quanta_series meta;
    quanta_sample previous, last;
    size_t channel_index;
    uint64_t next;
    int active, announced, dirty, has_last;
} quanta_binding;

/* Private batching only: no provider names, priorities, indexes or fallback. */
typedef struct quanta_acquisition
{
    const quanta_metric_desc *representative;
    const quanta_metric_desc **selected;
    quanta_metric_context context;
    quanta_status init_status, discovery_status;
    uint64_t refresh_count;
    int initialized;
} quanta_acquisition;

struct quanta_engine
{
    quanta_plan plan;
    quanta_catalog catalog;
    int has_run;
    quanta_paths paths;
    char roots[3][QUANTA_PATH_CAP];
    quanta_hal *hal;
    int owns_hal;
    quanta_acquisition *acquisitions;
    size_t acquisition_count;
    quanta_channel *channels;
    size_t channel_count, channel_cap;
    quanta_binding *bindings;
    size_t binding_count, binding_cap;
    uint64_t next_id, frame_id, discovery_epoch;
    size_t discovering_acquisition;
    quanta_mutex control_mutex;
    int stopped, force, has_errors;
    quanta_sink *sink;
};

uint64_t quanta_deadline_after(uint64_t now, uint64_t delay);
quanta_status quanta_engine_diagnostic(quanta_engine *, quanta_status, const char *);
int quanta_engine_stopping(quanta_engine *);
int quanta_engine_consume_refresh(quanta_engine *);
quanta_status quanta_engine_announce(quanta_engine *, quanta_binding *);
quanta_status quanta_engine_discover(quanta_engine *);
#endif
