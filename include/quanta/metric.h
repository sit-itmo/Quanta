/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_METRIC_H
#define QUANTA_METRIC_H
#include "types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct quanta_metric_context quanta_metric_context;
typedef struct quanta_instance quanta_instance;
typedef struct quanta_hal quanta_hal;
typedef struct quanta_discovery_sink quanta_discovery_sink;
typedef struct quanta_metric_desc quanta_metric_desc;
typedef quanta_status (*quanta_read_fn)(quanta_metric_context *, quanta_instance *, quanta_value *,
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
    /* One implementation per name. Lifecycle callbacks are part of the metric,
     * not an independently selectable provider. A shared callback tuple allows
     * the scheduler to acquire one snapshot for several related metric fields. */
    quanta_status (*init)(quanta_metric_context *);
    quanta_status (*enumerate)(quanta_metric_context *, quanta_discovery_sink *);
    quanta_status (*collect)(quanta_metric_context *);
    void (*destroy)(quanta_metric_context *);
    quanta_read_fn read;
    const char *arg; /* Implementation-defined field/attribute. */
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
    char resource[QUANTA_PATH_CAP]; /* Private metric locator; never a query
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

/* Selected descriptors belonging to one shared acquisition. No query or whole
 * registry crosses into metric implementation code. The scheduler serializes
 * callbacks. private_data is owned by the implementation and released by destroy;
 * init failure may leave partial data, so destroy must tolerate NULL/partial state. */
struct quanta_metric_context
{
    const quanta_metric_desc *const *metrics;
    size_t metric_count;
    quanta_paths paths;
    quanta_hal *hal;  /* Borrowed. A metric may use it, or bypass it completely. */
    void *private_data;
    uint64_t now_ns, acquisition_ns;
    char error[160];
};

struct quanta_discovery_sink
{
    void *context;
    quanta_status (*emit)(void *, const quanta_instance *);
};

int quanta_metric_wanted(const quanta_metric_context *, const quanta_metric_desc *);
quanta_status quanta_discover_single(quanta_metric_context *, quanta_discovery_sink *);
/* Small synchronous entry point used by MCU firmware. Labelled/multi-instance
 * metrics need an engine instead. HAL may be NULL for implementations bypassing it.
 * Return value and sample.status agree. init/destroy are balanced even on error. */
quanta_status quanta_metric_read_once(const quanta_metric_desc *, quanta_hal *,
                                    const quanta_paths *, quanta_sample *);

#ifdef __cplusplus
}
#endif
#endif
