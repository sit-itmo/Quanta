/* SPDX-License-Identifier: MIT */
#ifndef INV_VIEW_H
#define INV_VIEW_H
#include "inv.h"
#ifdef __cplusplus
extern "C" {
#endif
#define INV_HISTORY_MAX 8192
#define INV_HISTORY_BUDGET (64u*1024u*1024u)
typedef struct inv_point {uint64_t time_ns;double value;inv_status status;} inv_point;
typedef struct inv_item {
 inv_series meta;
 inv_sample last,good;
 int active,has_last,has_good;
 inv_point *history;
 size_t history_cap,history_count,history_head;
} inv_item;
typedef struct inv_model {
 inv_item *items;size_t count,cap,history_bytes;
 uint64_t history_ns,frames,samples;
 inv_frame_info frame;
 char diagnostic[256];
 inv_status end_status;
 int done,redact;
} inv_model;
void inv_model_init(inv_model *,const inv_plan *);
void inv_model_destroy(inv_model *);
inv_status inv_model_clone(inv_model *,const inv_model *);
inv_sink inv_model_sink(inv_model *);
inv_item *inv_model_find(inv_model *,uint64_t);
const inv_point *inv_history_at(const inv_item *,size_t);
void inv_item_value(char *,size_t,const inv_item *,int redact);
void inv_item_status(char *,size_t,const inv_item *,uint64_t now_ns);
const char *inv_metric_source(const inv_metric_desc *);
typedef struct inv_live inv_live;
inv_status inv_live_start(inv_engine *,const inv_plan *,inv_live **);
inv_status inv_live_pump(inv_live *,inv_sink *);
int inv_live_done(inv_live *);
inv_status inv_live_finish(inv_live *,int stop);
#ifdef __cplusplus
}
#endif
#endif
