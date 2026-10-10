/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_TYPES_H
#define QUANTA_TYPES_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>
#define QUANTA_VERSION "0.4.0"
#ifndef QUANTA_TEXT_CAP
#define QUANTA_TEXT_CAP 512
#endif
#ifndef QUANTA_KEY_CAP
#define QUANTA_KEY_CAP 128
#endif
#ifndef QUANTA_LABEL_CAP
#define QUANTA_LABEL_CAP 6
#endif
#ifndef QUANTA_NAME_CAP
#define QUANTA_NAME_CAP 48
#endif
#ifndef QUANTA_ID_CAP
#define QUANTA_ID_CAP 1024
#endif
#ifndef QUANTA_PATH_CAP
#define QUANTA_PATH_CAP 1024
#endif
#ifndef QUANTA_MAX_SELECT
#define QUANTA_MAX_SELECT 128
#endif
#ifndef QUANTA_MAX_CHANNELS
#define QUANTA_MAX_CHANNELS 4096
#endif
#ifndef QUANTA_MAX_SERIES
#define QUANTA_MAX_SERIES 2048
#endif
#ifndef QUANTA_QUERY_CAP
#define QUANTA_QUERY_CAP 65536
#endif
/* Canonical time units; ABI capacity macros must match in all translation
 * units. */
#define QUANTA_NS_MS UINT64_C(1000000)
#define QUANTA_NS_S UINT64_C(1000000000)

typedef enum quanta_status
{
    QUANTA_OK = 0,
    QUANTA_UNSUPPORTED,
    QUANTA_NOT_PRESENT,
    QUANTA_PERMISSION_DENIED,
    QUANTA_NO_DATA,
    QUANTA_WARMUP,
    QUANTA_GONE,
    QUANTA_INVALID_DATA,
    QUANTA_IO_ERROR,
    QUANTA_TIMEOUT,
    QUANTA_CANCELLED,
    QUANTA_NO_MEMORY,
    QUANTA_LIMIT,
    QUANTA_BAD_QUERY
} quanta_status;

typedef enum quanta_value_type
{
    QUANTA_U64,
    QUANTA_S64,
    QUANTA_F64,
    QUANTA_BOOL,
    QUANTA_STRING,
    QUANTA_BYTES
} quanta_value_type;

typedef enum quanta_metric_kind
{
    QUANTA_PROPERTY,
    QUANTA_GAUGE,
    QUANTA_COUNTER
} quanta_metric_kind;

typedef enum quanta_refresh_policy
{
    QUANTA_REFRESH_ONCE,
    QUANTA_REFRESH_POLL,
    QUANTA_REFRESH_ON_EVENT
} quanta_refresh_policy;

enum
{
    QUANTA_SENSITIVE = 1u,
    QUANTA_EXPENSIVE = 2u,
    QUANTA_QUALITY_ESTIMATED = 1u
};

typedef struct quanta_value
{
    quanta_value_type type;

    union
    {
        uint64_t u64;
        int64_t s64;
        double f64;
        int boolean;
        char string[QUANTA_TEXT_CAP];

        struct
        {
            size_t size;
            uint8_t data[256];
        } bytes;
    } as;
} quanta_value;

typedef struct quanta_label
{
    char key[QUANTA_NAME_CAP];
    char value[QUANTA_TEXT_CAP];
} quanta_label;

/* Status/quality belong to quanta_sample; values own all fixed-size storage. */
typedef struct quanta_paths
{
    const char *proc_root;
    const char *sys_root;
    const char *etc_root;
} quanta_paths;

const char *quanta_status_name(quanta_status status);
const char *quanta_type_name(quanta_value_type type);
const char *quanta_kind_name(quanta_metric_kind kind);

#ifdef __cplusplus
}
#endif
#endif
