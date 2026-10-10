/* SPDX-License-Identifier: MIT -- private to metric implementations. */
#ifndef QUANTA_SNAPSHOT_CONTEXT_H
#define QUANTA_SNAPSHOT_CONTEXT_H
#include "quanta/types.h"

typedef struct quanta_snapshot_context
{
    quanta_paths paths;
    char *error;
    size_t error_capacity;
} quanta_snapshot_context;
#endif
