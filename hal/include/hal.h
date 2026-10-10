/* SPDX-License-Identifier: MIT -- acquisition context, independent of
 * registry/query/frontend */
#ifndef QUANTA_HAL_H
#define QUANTA_HAL_H
#include "quanta/types.h"

typedef struct quanta_hal_context
{
    quanta_paths paths;
    char *error;
    size_t error_capacity;
} quanta_hal_context;

/* Passed by value. Paths and the optional diagnostic buffer are borrowed during
 * the call. No provider, catalog, query, engine or frontend object crosses this
 * boundary. */
#endif
