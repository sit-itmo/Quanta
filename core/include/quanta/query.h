/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_QUERY_H
#define QUANTA_QUERY_H
#include "metric.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum quanta_mode
{
    QUANTA_MODE_FILE,
    QUANTA_MODE_TUI,
    QUANTA_MODE_GUI
} quanta_mode;

typedef enum quanta_format
{
    QUANTA_FORMAT_HUMAN,
    QUANTA_FORMAT_TABLE,
    QUANTA_FORMAT_CSV,
    QUANTA_FORMAT_MD,
    QUANTA_FORMAT_VALUE
} quanta_format;

typedef enum quanta_widget
{
    QUANTA_WIDGET_VALUE,
    QUANTA_WIDGET_TEXT,
    QUANTA_WIDGET_BAR,
    QUANTA_WIDGET_GAUGE,
    QUANTA_WIDGET_PLOT,
    QUANTA_WIDGET_TABLE
} quanta_widget;

typedef enum quanta_transform
{
    QUANTA_RAW,
    QUANTA_RATE,
    QUANTA_DELTA
} quanta_transform;

typedef struct quanta_view_spec
{
    quanta_widget widget;
    char title[128], unit[32];
    double min, max;
    int has_min, has_max, precision;
} quanta_view_spec;

typedef struct quanta_select
{
    char pattern[QUANTA_KEY_CAP];
    char provider[QUANTA_NAME_CAP]; /* Empty: first registered compatible
                                       provider. */
    quanta_label labels[QUANTA_LABEL_CAP];
    size_t label_count;
    int optional;
    quanta_transform transform;
    uint64_t period_ns;
    quanta_view_spec view;
} quanta_select;

typedef struct quanta_plan
{
    quanta_mode mode;
    quanta_format format;
    int once, once_explicit, strict_poll, redact, fail_on_error, csv_safe;
    uint64_t period_ns, redraw_ns, history_ns, duration_ns, count, warmup_ns;
    unsigned columns;
    char output[QUANTA_PATH_CAP];
    char font_path[QUANTA_PATH_CAP];
    quanta_select selects[QUANTA_MAX_SELECT];
    size_t select_count;
} quanta_plan;

struct quanta_catalog;
void quanta_plan_init(quanta_plan *plan);
quanta_status quanta_query_parse(const char *source, quanta_plan *plan, char *error,
                                 size_t capacity);
quanta_status quanta_plan_validate_with_catalog(quanta_plan *plan,
                                                const struct quanta_catalog *catalog, char *error,
                                                size_t capacity);
quanta_status quanta_parse_duration(const char *text, uint64_t *nanoseconds);
int quanta_select_matches_metric(const quanta_select *select, const quanta_metric_desc *metric);
int quanta_select_matches(const quanta_select *select, const quanta_instance *instance);

#ifdef __cplusplus
}
#endif
#endif
