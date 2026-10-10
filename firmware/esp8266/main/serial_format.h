/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_SERIAL_FORMAT_H
#define QUANTA_SERIAL_FORMAT_H
#include "quanta/metric.h"
int quanta_serial_value(char *out, size_t capacity, const quanta_value *value);
#endif
