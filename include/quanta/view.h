/**
 * @file include/quanta/view.h
 *
 * Value/scale presentation structures and functions, not measurement acquisition.
 *
 * Sample: the value of a particular series together with its timestamp, status and quality
 * flags. An error must not be presented as a valid zero.
 *
 * History: bounded storage for a series of samples. The latest sample and the latest successful
 * sample may differ.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_VIEW_H
#define QUANTA_VIEW_H
#include "quanta/quanta.h"

#ifdef __cplusplus
extern "C"
{
#endif

void quanta_item_value(char *out, size_t capacity, const quanta_item *item, int redact);
void quanta_item_status(char *out, size_t capacity, const quanta_item *item, uint64_t now_ns);
void quanta_format_value(char *out, size_t capacity, const quanta_value *value,
                         const char *canonical_unit, const quanta_view_spec *view, int machine);
void quanta_series_name(char *out, size_t capacity, const quanta_series *series);
const char *quanta_mode_name(quanta_mode mode);
int quanta_mode_available(quanta_mode mode);
int quanta_run_frontend(quanta_engine *engine, const quanta_plan *plan);
int quanta_run_file(quanta_engine *engine, const quanta_plan *plan);
int quanta_run_tui(quanta_engine *engine, const quanta_plan *plan);
int quanta_run_gui(quanta_engine *engine, const quanta_plan *plan);

#ifdef __cplusplus
}
#endif
#endif
