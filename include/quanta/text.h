/**
 * @file include/quanta/text.h
 *
 * Shared operations on bounded strings, labels, name patterns and numeric input.
 *
 * Labels: key/value pairs selecting an instance, such as cpu="0". They describe the source, not
 * a widget caption.
 *
 * Series: a particular metric instance with labels and an identity. Replacing a device may start
 * a new series even when its name is unchanged.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_TEXT_H
#define QUANTA_TEXT_H
#include "types.h"

#ifdef __cplusplus
extern "C"
{
#endif

int quanta_copy(char *destination, size_t capacity, const char *source);
void quanta_trim(char *text);
void quanta_safe_text(char *destination, size_t capacity, const char *source);
int quanta_parse_u64(const char *text, uint64_t *value);
int quanta_parse_s64(const char *text, int64_t *value);
int quanta_glob_match(const char *pattern, const char *text);
const char *quanta_label_get(const quanta_label *labels, size_t count, const char *key);
int quanta_label_set(quanta_label *labels, size_t *count, const char *key, const char *value);

#ifdef __cplusplus
}
#endif
#endif
