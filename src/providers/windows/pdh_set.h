/* SPDX-License-Identifier: MIT -- a bounded, locale-independent PDH counter set */
#ifndef INV_PDH_SET_H
#define INV_PDH_SET_H
#include "windows_internal.h"
#include <pdh.h>
#include <pdhmsg.h>
typedef struct inv_pdh_item {
    wchar_t *path;
    char name[INV_TEXT_CAP];
    PDH_HCOUNTER counter;
    double value;
    inv_status status;
    uint64_t collected_ns,window_ns;
    unsigned seen,samples;
} inv_pdh_item;
typedef struct inv_pdh_set {
    PDH_HQUERY query;
    const wchar_t *english_path;
    inv_pdh_item *items;
    size_t count,capacity;
    int delta_counter;
} inv_pdh_set;
inv_status inv_pdh_status(PDH_STATUS);
inv_status inv_pdh_open(inv_pdh_set *,const wchar_t *,int delta_counter);
inv_status inv_pdh_discover(inv_pdh_set *);
inv_status inv_pdh_collect(inv_pdh_set *);
const inv_pdh_item *inv_pdh_find(const inv_pdh_set *,const char *);
void inv_pdh_close(inv_pdh_set *);
#endif
