/* SPDX-License-Identifier: MIT
 * C11 descriptor initializer helpers, not a second registration/runtime layer.
 * Each helper owns a disjoint set of fields: do not override the same field
 * later in an initializer. Raw designated initializers remain supported.
 */
#ifndef QUANTA_METRIC_DEF_H
#define QUANTA_METRIC_DEF_H
#include "metric.h"

/* Stable identity. Documentation follows docs/metrics/<query-name>.md. */
#define QUANTA_META_VERSION(name_, title_, description_, version_)                      \
    .key = (name_), .title = (title_), .description = (description_),                                \
    .doc_path = "docs/metrics/" name_ ".md", .semantic_version = (version_)
#define QUANTA_META_DESC(name_, title_, description_) \
    QUANTA_META_VERSION(name_, title_, description_, 1)
#define QUANTA_META(name_, title_) QUANTA_META_DESC(name_, title_, title_)

/* Low-level form for an unusual type/kind/event policy; timing is separate. */
#define QUANTA_VALUE(type_, kind_, refresh_, unit_)                                                 \
    .type = (type_), .kind = (kind_), .refresh = (refresh_), .unit = (unit_)
#define QUANTA_TIMING_MS(minimum_, recommended_, stale_)                                            \
    .min_poll_ns = (uint64_t)(minimum_) * QUANTA_NS_MS,                                              \
    .recommended_poll_ns = (uint64_t)(recommended_) * QUANTA_NS_MS,                                  \
    .stale_after_ns = (uint64_t)(stale_) * QUANTA_NS_MS

/* Properties are acquired once; stale=0 means no automatic stale deadline. */
#define QUANTA_PROPERTY_VALUE(type_, unit_, stale_)                                                       \
    QUANTA_VALUE(type_, QUANTA_PROPERTY, QUANTA_REFRESH_ONCE, unit_),                                \
    QUANTA_TIMING_MS(0, 1000, stale_)
#define QUANTA_PROPERTY_TEXT(stale_) QUANTA_PROPERTY_VALUE(QUANTA_STRING, "", stale_)
#define QUANTA_PROPERTY_BOOL(stale_) QUANTA_PROPERTY_VALUE(QUANTA_BOOL, "", stale_)
#define QUANTA_PROPERTY_U64(unit_, stale_) QUANTA_PROPERTY_VALUE(QUANTA_U64, unit_, stale_)

/* Polling templates: recommended 1 s, stale after 5 s, explicit minimum period. */
#define QUANTA_GAUGE_U64(unit_, minimum_)                                                           \
    QUANTA_VALUE(QUANTA_U64, QUANTA_GAUGE, QUANTA_REFRESH_POLL, unit_),                               \
    QUANTA_TIMING_MS(minimum_, 1000, 5000)
#define QUANTA_GAUGE_F64(unit_, minimum_)                                                           \
    QUANTA_VALUE(QUANTA_F64, QUANTA_GAUGE, QUANTA_REFRESH_POLL, unit_),                               \
    QUANTA_TIMING_MS(minimum_, 1000, 5000)
#define QUANTA_GAUGE_BYTES(minimum_) QUANTA_GAUGE_U64("B", minimum_)
#define QUANTA_GAUGE_RATIO(minimum_) QUANTA_GAUGE_F64("1", minimum_)
#define QUANTA_GAUGE_SECONDS(minimum_) QUANTA_GAUGE_F64("s", minimum_)
#define QUANTA_COUNTER_U64(unit_, minimum_)                                                         \
    QUANTA_VALUE(QUANTA_U64, QUANTA_COUNTER, QUANTA_REFRESH_POLL, unit_),                             \
    QUANTA_TIMING_MS(minimum_, 1000, 5000), .counter_bits = 64
#define QUANTA_COUNTER_BYTES(minimum_) QUANTA_COUNTER_U64("B", minimum_)

/* One acquisition callback tuple, never a selectable provider. */
#define QUANTA_SNAPSHOT(prefix_, field_)                                                            \
    .init = prefix_ ## _init, .enumerate = prefix_ ## _enumerate,                                    \
    .collect = prefix_ ## _collect, .destroy = prefix_ ## _destroy,                                 \
    .read = prefix_ ## _read, .arg = (field_)
/* Immutable data collected during init (e.g. a pinned CPUID snapshot). */
#define QUANTA_INIT_SNAPSHOT(prefix_, field_)                                                       \
    .init = prefix_ ## _init, .enumerate = prefix_ ## _enumerate,                                    \
    .destroy = prefix_ ## _destroy, .read = prefix_ ## _read, .arg = (field_)
#define QUANTA_SINGLE_READ(reader_, field_)                                                        \
    .enumerate = quanta_discover_single, .read = (reader_), .arg = (field_)

/* Empty labels/source strings are valid; NULL is deliberately not substituted. */
#define QUANTA_SOURCE(labels_, source_) .label_schema = (labels_), .dependencies = (source_)
#endif
