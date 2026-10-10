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
