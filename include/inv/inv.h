/* SPDX-License-Identifier: MIT
 * Public C99 interface. Structures own their fixed-size value storage.
 * Strings are UTF-8 unless documented otherwise. No C++/OS types here.
 */
#ifndef INV_INV_H
#define INV_INV_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#define INV_VERSION "0.2.2"
#define INV_TEXT_CAP 512
#define INV_KEY_CAP 128
#define INV_LABEL_CAP 6
#define INV_NAME_CAP 48
#define INV_ID_CAP 1024
#define INV_PATH_CAP 1024
#define INV_MAX_SELECT 128
#define INV_MAX_CHANNELS 4096
#define INV_MAX_SERIES 2048
#define INV_QUERY_CAP 65536
#define INV_NS_MS UINT64_C(1000000)
#define INV_NS_S UINT64_C(1000000000)

typedef enum inv_status {
 INV_OK=0, INV_UNSUPPORTED, INV_NOT_PRESENT, INV_PERMISSION_DENIED,
 INV_NO_DATA, INV_WARMUP, INV_GONE, INV_INVALID_DATA, INV_IO_ERROR,
 INV_TIMEOUT, INV_CANCELLED, INV_NO_MEMORY, INV_LIMIT, INV_BAD_QUERY
} inv_status;

typedef enum inv_value_type { INV_U64, INV_S64, INV_F64, INV_BOOL,
 INV_STRING, INV_BYTES } inv_value_type;

typedef enum inv_metric_kind { INV_PROPERTY, INV_GAUGE, INV_COUNTER } inv_metric_kind;
typedef enum inv_refresh_policy 
{ 
    INV_REFRESH_ONCE, 
    INV_REFRESH_POLL,
    INV_REFRESH_ON_EVENT } inv_refresh_policy;

enum { INV_SENSITIVE=1u, INV_EXPENSIVE=2u, INV_QUALITY_ESTIMATED=1u };

typedef struct inv_value 
{
    inv_value_type type;
    union 
    { 
        int64_t s64; 
        double f64;
        struct { size_t size; char data[256]; } string;
        struct { size_t size; uint8_t data[256]; } bytes;
    } as;
}
inv_value;
typedef struct inv_label { char key[INV_NAME_CAP]; char value[INV_TEXT_CAP]; } inv_label;
typedef struct inv_read_ctx inv_read_ctx;
typedef struct inv_instance inv_instance;
typedef struct inv_provider_desc inv_provider_desc;
typedef struct inv_metric_desc inv_metric_desc;
typedef inv_status (*inv_read_fn)(inv_read_ctx *, inv_instance *, inv_value *,
 uint64_t *window_ns, uint32_t *quality);
struct inv_metric_desc {
 const char *key, *title, *description, *doc_path;
 uint32_t semantic_version;
 inv_value_type type;
 inv_metric_kind kind;
 inv_refresh_policy refresh;
 const char *unit;
 uint64_t min_poll_ns, recommended_poll_ns, stale_after_ns;
 uint32_t flags, counter_bits;
 const char *label_schema; /* Comma-separated allowed label names. */
 const char *dependencies; /* Named source dependencies, for introspection. */
 const inv_provider_desc *provider;
 inv_read_fn read;
 const char *arg;          /* Provider-defined field/attribute. */
};
struct inv_instance {
 uint64_t series_id;
 uint32_t generation;
 uint64_t min_poll_ns; /* Optional device-specific floor discovered at runtime. */
 const inv_metric_desc *metric;
 char entity_key[INV_ID_CAP];
 inv_label labels[INV_LABEL_CAP];
 size_t label_count;
 char resource[INV_PATH_CAP]; /* Private provider locator; never a query path. */
 uint64_t reader_state[16];   /* Owned per-instance state, e.g. CPU baseline. */
};
typedef struct inv_sample {
 uint64_t series_id, sequence, measured_at_ns, read_duration_ns, window_ns;
 uint64_t counter_epoch;
 inv_status status;
 inv_value value;
 uint32_t quality_flags;
 char message[160];
} inv_sample;
typedef enum inv_mode { INV_MODE_FILE, INV_MODE_TUI, INV_MODE_GUI } inv_mode;
typedef enum inv_format { INV_FORMAT_HUMAN, INV_FORMAT_TABLE, INV_FORMAT_CSV,
 INV_FORMAT_MD } inv_format;
typedef enum inv_widget { INV_WIDGET_VALUE, INV_WIDGET_TEXT, INV_WIDGET_BAR,
 INV_WIDGET_GAUGE, INV_WIDGET_PLOT, INV_WIDGET_TABLE } inv_widget;
typedef enum inv_transform { INV_RAW, INV_RATE, INV_DELTA } inv_transform;
typedef struct inv_view_spec {
 inv_widget widget;
 char title[128], unit[32];
 double min, max;
 int has_min, has_max, precision;
} inv_view_spec;
typedef struct inv_select {
 char pattern[INV_KEY_CAP];
 inv_label labels[INV_LABEL_CAP];
 size_t label_count;
 int optional;
 inv_transform transform;
 uint64_t period_ns;
 inv_view_spec view;
} inv_select;
typedef struct inv_plan {
 inv_mode mode;
 inv_format format;
 int once, once_explicit, strict_poll, redact, fail_on_error, csv_safe;
 uint64_t period_ns, redraw_ns, history_ns, duration_ns, count, warmup_ns;
 unsigned columns;
 char output[INV_PATH_CAP];
 char font_path[INV_PATH_CAP];
 inv_select selects[INV_MAX_SELECT];
 size_t select_count;
} inv_plan;
typedef struct inv_series {
 uint64_t id, source_id;
 uint32_t generation;
 unsigned selection_index;
 const inv_metric_desc *metric;
 inv_value_type type;
 inv_transform transform;
 char key[INV_KEY_CAP+16], unit[32], entity_key[INV_ID_CAP];
 inv_label labels[INV_LABEL_CAP];
 size_t label_count;
 inv_view_spec view;
 uint64_t effective_period_ns;
} inv_series;
typedef struct inv_frame_info {
 uint64_t id, monotonic_ns, realtime_ns;
} inv_frame_info;
typedef struct inv_sink {
 uint32_t struct_size, api_version;
 void *context;
 inv_status (*begin_session)(void *, const inv_plan *);
 inv_status (*upsert_series)(void *, const inv_series *);
 inv_status (*remove_series)(void *, uint64_t);
 inv_status (*begin_frame)(void *, const inv_frame_info *);
 inv_status (*emit_sample)(void *, const inv_sample *);
 inv_status (*end_frame)(void *);
 inv_status (*emit_diagnostic)(void *, inv_status, const char *);
 inv_status (*emit_progress)(void *, uint32_t, uint32_t, const char *);
 inv_status (*end_session)(void *, inv_status);
} inv_sink;
typedef struct inv_paths {
 const char *proc_root, *sys_root, *etc_root;
} inv_paths;
typedef struct inv_engine inv_engine;

/* Opens a UTF-8 path on both platforms; caller owns FILE*. */
FILE *inv_fopen_utf8(const char *path,const char *mode);
const char *inv_status_name(inv_status);
const char *inv_type_name(inv_value_type);
const char *inv_kind_name(inv_metric_kind);
const inv_metric_desc *const *inv_registry(size_t *count);
const inv_metric_desc *inv_metric_find(const char *key);
const char *inv_metric_documentation(const char *key);
inv_status inv_registry_validate(char *error, size_t cap);
void inv_plan_init(inv_plan *);
inv_status inv_query_parse(const char *, inv_plan *, char *, size_t);
inv_status inv_plan_validate(inv_plan *, char *, size_t);
inv_status inv_parse_duration(const char *, uint64_t *);
int inv_glob_match(const char *, const char *);
int inv_select_matches_metric(const inv_select *, const inv_metric_desc *);
int inv_select_matches(const inv_select *, const inv_instance *);
const char *inv_label_get(const inv_label *, size_t, const char *);
int inv_label_set(inv_label *, size_t *, const char *, const char *);
uint64_t inv_now_ns(void);
uint64_t inv_realtime_ns(void);
void inv_sleep_ns(uint64_t);
void inv_engine_stop(inv_engine *);
void inv_engine_refresh(inv_engine *);
inv_status inv_engine_create(const inv_plan *, const inv_paths *, inv_engine **,
 char *error, size_t cap);
inv_status inv_engine_run(inv_engine *, inv_sink *);
void inv_engine_destroy(inv_engine *);
void inv_engine_explain(const inv_engine *, FILE *);
int inv_engine_has_sample_errors(const inv_engine *);
uint64_t inv_engine_provider_refreshes(const inv_engine *,const char *provider_id);
int inv_run_file(inv_engine *, const inv_plan *);
int inv_run_tui(inv_engine *, const inv_plan *);
int inv_run_gui(inv_engine *, const inv_plan *);
int inv_install_signal_handlers(void);
int inv_interrupted(void);
int inv_copy(char *dst, size_t cap, const char *src);
void inv_safe_text(char *, size_t, const char *);
void inv_format_value(char *, size_t, const inv_value *, const char *canonical_unit,
 const inv_view_spec *, int machine);
int inv_value_number(const inv_value *, double *);
int inv_unit_scale(const char *canonical, const char *display, double *scale);
void inv_series_name(char *, size_t, const inv_series *);
void inv_utc_string(uint64_t, char *, size_t);
/* All arguments supplied to callbacks are borrowed for that callback only.
 * Copy inv_sample / inv_series by value to retain them. Descriptor pointers are
 * process-lifetime constants. Never retain inv_plan supplied to begin_session.
 */
#ifdef __cplusplus
}
#endif
#endif
