/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_METRIC_H
#define QUANTA_METRIC_H
#include "types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct quanta_read_ctx quanta_read_ctx;
typedef struct quanta_instance quanta_instance;
typedef struct quanta_provider_desc quanta_provider_desc;
typedef struct quanta_metric_desc quanta_metric_desc;
typedef quanta_status (*quanta_read_fn)(quanta_read_ctx *, quanta_instance *, quanta_value *,
                                        uint64_t *window_ns, uint32_t *quality);

struct quanta_metric_desc
{
    const char *key, *title, *description, *doc_path;
    uint32_t semantic_version;
    quanta_value_type type;
    quanta_metric_kind kind;
    quanta_refresh_policy refresh;
    const char *unit;
    uint64_t min_poll_ns, recommended_poll_ns, stale_after_ns;
    uint32_t flags, counter_bits;
    const char *label_schema; /* Comma-separated allowed label names. */
    const char *dependencies; /* Named source dependencies, for introspection. */
    const quanta_provider_desc *provider;
    quanta_read_fn read;
    const char *arg; /* Provider-defined field/attribute. */
};

struct quanta_instance
{
    uint64_t series_id;
    uint32_t generation;
    uint64_t min_poll_ns; /* Optional device-specific floor discovered at runtime. */
    const quanta_metric_desc *metric;
    char entity_key[QUANTA_ID_CAP];
    quanta_label labels[QUANTA_LABEL_CAP];
    size_t label_count;
    char resource[QUANTA_PATH_CAP]; /* Private provider locator; never a query
                                       path. */
    uint64_t reader_state[16];      /* Owned per-instance state, e.g. CPU baseline. */
};

typedef struct quanta_sample
{
    uint64_t series_id, sequence, measured_at_ns, read_duration_ns, window_ns;
    uint64_t counter_epoch;
    quanta_status status;
    quanta_value value;
    uint32_t quality_flags;
    char message[160];
} quanta_sample;

#ifdef __cplusplus
}
#endif
#endif
