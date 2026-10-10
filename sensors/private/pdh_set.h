/**
 * @file sensors/private/pdh_set.h
 *
 * Windows PDH query/counter ownership, instances and sample statuses. This header is private,
 * not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT -- a bounded, locale-independent PDH counter set
 */
#ifndef QUANTA_PDH_SET_H
#define QUANTA_PDH_SET_H
#include "windows_internal.h"
#include <pdh.h>
#include <pdhmsg.h>

typedef struct quanta_pdh_item
{
    wchar_t *path;
    char name[QUANTA_TEXT_CAP];
    PDH_HCOUNTER counter;
    double value;
    quanta_status status;
    uint64_t collected_ns, window_ns;
    unsigned seen, samples;
} quanta_pdh_item;

typedef struct quanta_pdh_set
{
    PDH_HQUERY query;
    const wchar_t *english_path;
    quanta_pdh_item *items;
    size_t count, capacity;
    int delta_counter;
} quanta_pdh_set;

quanta_status quanta_pdh_status(PDH_STATUS);
quanta_status quanta_pdh_open(quanta_pdh_set *, const wchar_t *, int delta_counter);
quanta_status quanta_pdh_discover(quanta_pdh_set *);
quanta_status quanta_pdh_collect(quanta_pdh_set *);
const quanta_pdh_item *quanta_pdh_find(const quanta_pdh_set *, const char *);
void quanta_pdh_close(quanta_pdh_set *);
#endif
