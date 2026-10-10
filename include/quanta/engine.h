/**
 * @file include/quanta/engine.h
 *
 * Public engine API, series descriptions and sink callbacks. The core is independent of built-in
 * collectors.
 *
 * Plan: a parsed query containing selected metrics, polling intervals, history, HAL and
 * presentation settings. Parsing does not access hardware.
 *
 * Series: a particular metric instance with labels and an identity. Replacing a device may start
 * a new series even when its name is unchanged.
 *
 * Sink: a set of engine notification callbacks that decouples data collection from file output,
 * TUI and GUI.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_ENGINE_H
#define QUANTA_ENGINE_H
#include "query.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "quanta/osal.h"

typedef struct quanta_series
{
    uint64_t id, source_id;
    uint32_t generation;
    unsigned selection_index;
    const quanta_metric_desc *metric;
    quanta_value_type type;
    quanta_transform transform;
    char key[QUANTA_KEY_CAP + 16], unit[32], entity_key[QUANTA_ID_CAP];
    quanta_label labels[QUANTA_LABEL_CAP];
    size_t label_count;
    quanta_view_spec view;
    uint64_t effective_period_ns;
} quanta_series;

typedef struct quanta_frame_info
{
    uint64_t id, monotonic_ns, realtime_ns;
} quanta_frame_info;

typedef struct quanta_sink
{
    uint32_t struct_size, api_version;
    void *context;
    quanta_status (*begin_session)(void *, const quanta_plan *);
    quanta_status (*upsert_series)(void *, const quanta_series *);
    quanta_status (*remove_series)(void *, uint64_t);
    quanta_status (*begin_frame)(void *, const quanta_frame_info *);
    quanta_status (*emit_sample)(void *, const quanta_sample *);
    quanta_status (*end_frame)(void *);
    quanta_status (*emit_diagnostic)(void *, quanta_status, const char *);
    quanta_status (*emit_progress)(void *, uint32_t, uint32_t, const char *);
    quanta_status (*end_session)(void *, quanta_status);
} quanta_sink;

typedef struct quanta_engine quanta_engine;
/* Catalog/descriptors and their strings must outlive the engine and every
 * retained sample model. The plan and paths are copied. The engine may run
 * once; destroy after the worker has joined. stop/refresh are the only engine
 * calls allowed concurrently with run. */
void quanta_series_name(char *destination, size_t capacity, const quanta_series *series);
typedef struct quanta_engine_options
{
    const quanta_paths *paths;
    /* Optional caller-owned HAL. It must outlive the engine. When NULL the
     * engine opens the HAL named by plan.hal, read-only, and owns that handle. */
    quanta_hal *hal;
} quanta_engine_options;

quanta_status quanta_engine_create_with_options(const quanta_plan *,
                                               const struct quanta_catalog *,
                                               const quanta_engine_options *,
                                               quanta_engine **, char *, size_t);
quanta_status quanta_engine_create_with_catalog(const quanta_plan *plan, const quanta_paths *paths,
                                                const struct quanta_catalog *catalog,
                                                quanta_engine **out, char *error, size_t capacity);
quanta_status quanta_engine_run(quanta_engine *engine, quanta_sink *sink);
void quanta_engine_stop(quanta_engine *engine);
void quanta_engine_refresh(quanta_engine *engine);
void quanta_engine_destroy(quanta_engine *engine);
void quanta_engine_explain(const quanta_engine *engine, quanta_file *out);
int quanta_engine_has_sample_errors(const quanta_engine *engine);
uint64_t quanta_engine_metric_acquisitions(const quanta_engine *engine, const char *metric_key);
/* Sink arguments are borrowed during the callback. Samples/series may be copied
 * by value, but their metric descriptor pointer still borrows the catalog. Sink
 * callbacks must not recursively run or destroy the engine. An error from a
 * callback stops collection. */

#ifdef __cplusplus
}
#endif
#endif
