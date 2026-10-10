/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_HISTORY_H
#define QUANTA_HISTORY_H
#include "engine.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define QUANTA_HISTORY_MAX 8192
#define QUANTA_HISTORY_BUDGET (64u * 1024u * 1024u)

typedef struct quanta_point
{
    uint64_t time_ns;
    double value;
    quanta_status status;
} quanta_point;

typedef struct quanta_item
{
    quanta_series meta;
    quanta_sample last, good;
    int active, has_last, has_good;
    quanta_point *history;
    size_t history_cap, history_count, history_head;
} quanta_item;

typedef struct quanta_model
{
    quanta_item *items;
    size_t count, cap, history_bytes;
    uint64_t history_ns, frames, samples;
    quanta_frame_info frame;
    char diagnostic[256];
    quanta_status end_status;
    int done, redact;
} quanta_model;

void quanta_model_init(quanta_model *, const quanta_plan *);
void quanta_model_destroy(quanta_model *);
quanta_status quanta_model_clone(quanta_model *, const quanta_model *);
quanta_sink quanta_model_sink(quanta_model *);
quanta_item *quanta_model_find(quanta_model *, uint64_t);
const quanta_point *quanta_history_at(const quanta_item *, size_t);

#ifdef __cplusplus
}
#endif
#endif
