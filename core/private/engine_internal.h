/* SPDX-License-Identifier: MIT -- private scheduler state, never installed */
#ifndef QUANTA_ENGINE_INTERNAL_H
#define QUANTA_ENGINE_INTERNAL_H
#include "core_internal.h"

typedef struct quanta_channel
{
    quanta_instance in;
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

struct quanta_engine
{
    quanta_plan plan;
    quanta_catalog catalog;
    int has_run;
    unsigned char provider_opened[QUANTA_PROVIDER_COUNT];
    quanta_paths paths;
    char roots[3][QUANTA_PATH_CAP];
    quanta_read_ctx ctx[QUANTA_PROVIDER_COUNT];
    const quanta_provider_desc *providers[QUANTA_PROVIDER_COUNT];
    quanta_status open_status[QUANTA_PROVIDER_COUNT], discovery_status[QUANTA_PROVIDER_COUNT];
    uint64_t refresh_count[QUANTA_PROVIDER_COUNT];
    quanta_channel *channels;
    size_t channel_count, channel_cap;
    quanta_binding *bindings;
    size_t binding_count, binding_cap;
    uint64_t next_id, frame_id, discovery_epoch;
    const quanta_provider_desc *discovering_provider;
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
