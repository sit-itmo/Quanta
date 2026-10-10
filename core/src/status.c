/**
 * @file core/src/status.c
 *
 * Readable names for statuses, value types and metric kinds used in diagnostics and output.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 *
 * Sample: the value of a particular series together with its timestamp, status and quality
 * flags. An error must not be presented as a valid zero.
 */
/* SPDX-License-Identifier: MIT */
#include "core_internal.h"

const char *quanta_status_name(quanta_status s)
{
    static const char *const n[] = {"OK",       "UNSUPPORTED", "NOT_PRESENT", "PERMISSION_DENIED",
                                    "NO_DATA",  "WARMUP",      "GONE",        "INVALID_DATA",
                                    "IO_ERROR", "TIMEOUT",     "CANCELLED",   "NO_MEMORY",
                                    "LIMIT",    "BAD_QUERY"};
    return (unsigned)s < QUANTA_ARRAY_SIZE(n) ? n[s] : "UNKNOWN";
}

const char *quanta_type_name(quanta_value_type t)
{
    static const char *const n[] = {"u64", "s64", "f64", "bool", "string", "bytes"};
    return (unsigned)t < QUANTA_ARRAY_SIZE(n) ? n[t] : "unknown";
}

const char *quanta_kind_name(quanta_metric_kind k)
{
    return k == QUANTA_PROPERTY ? "property" : k == QUANTA_GAUGE ? "gauge" : "counter";
}
