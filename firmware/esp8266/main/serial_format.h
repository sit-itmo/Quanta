/**
 * @file firmware/esp8266/main/serial_format.h
 *
 * Interface for formatting a single serial value into a bounded output buffer.
 *
 * Sample: the value of a particular series together with its timestamp, status and quality
 * flags. An error must not be presented as a valid zero.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_SERIAL_FORMAT_H
#define QUANTA_SERIAL_FORMAT_H
#include "quanta/metric.h"
int quanta_serial_value(char *out, size_t capacity, const quanta_value *value);
#endif
