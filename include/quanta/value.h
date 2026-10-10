/**
 * @file include/quanta/value.h
 *
 * Public functions for value validation, unit conversion and counter-derived values.
 *
 * Sample: the value of a particular series together with its timestamp, status and quality
 * flags. An error must not be presented as a valid zero.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_VALUE_H
#define QUANTA_VALUE_H
#include "engine.h"

#ifdef __cplusplus
extern "C"
{
#endif

int quanta_value_number(const quanta_value *value, double *number);
int quanta_unit_scale(const char *canonical, const char *display, double *scale);
quanta_status quanta_value_validate(const quanta_value *value);
quanta_status quanta_counter_derive(quanta_transform transform, const quanta_sample *previous,
                                    const quanta_sample *current, quanta_value *value,
                                    uint64_t *window_ns);

#ifdef __cplusplus
}
#endif
#endif
