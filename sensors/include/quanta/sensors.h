/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_SENSORS_H
#define QUANTA_SENSORS_H
#include "quanta/quanta.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Convenience entry points for the compiled desktop catalog. These are
 * implemented by quanta_sensors, not by the reusable quanta_core. Mock/custom
 * catalogs need only core. */
const quanta_catalog *quanta_builtin_catalog(void);
const quanta_metric_desc *const *quanta_registry(size_t *count);
const quanta_metric_desc *quanta_metric_find(const char *key);
const char *quanta_metric_documentation(const char *key);
quanta_status quanta_registry_validate(char *error, size_t capacity);
quanta_status quanta_plan_validate(quanta_plan *plan, char *error, size_t capacity);
quanta_status quanta_engine_create(const quanta_plan *plan, const quanta_paths *paths,
                                   quanta_engine **out, char *error, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
