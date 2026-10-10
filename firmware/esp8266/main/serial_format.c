/* SPDX-License-Identifier: MIT
 * No printf 64-bit/float format support is required from embedded newlib. */
#include "serial_format.h"
#include "quanta/text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int decimal(char *out, size_t capacity, uint64_t value)
{
    char reverse[20];
    size_t n = 0, i;
    do { reverse[n++] = (char)('0' + value % 10u); value /= 10u; } while (value);
    if (capacity <= n) { if (capacity) out[0] = 0; return 0; }
    for (i = 0; i < n; ++i) out[i] = reverse[n - 1 - i];
    out[n] = 0;
    return 1;
}
int quanta_serial_value(char *out, size_t capacity, const quanta_value *value)
{
    char text[64];
    uint64_t magnitude;
    size_t offset = 0;
    if (!out || !capacity || !value) return 0;
    out[0] = 0;
    switch (value->type)
    {
    case QUANTA_U64: return decimal(out, capacity, value->as.u64);
    case QUANTA_S64:
        if (value->as.s64 < 0)
        {
            text[offset++] = '-';
            magnitude = (uint64_t)(-(value->as.s64 + 1)) + 1u;
        }
        else magnitude = (uint64_t)value->as.s64;
        if (!decimal(text + offset, sizeof(text) - offset, magnitude)) return 0;
        return quanta_copy(out, capacity, text);
    case QUANTA_F64:
        if (!isfinite(value->as.f64) || fabs(value->as.f64) >= 1.0e12) return 0;
        if (signbit(value->as.f64)) text[offset++] = '-';
        magnitude = (uint64_t)(fabs(value->as.f64) * 1000000.0 + 0.5);
        if (!decimal(text + offset, sizeof(text) - offset, magnitude / 1000000u)) return 0;
        offset = strlen(text);
        snprintf(text + offset, sizeof(text) - offset, ".%06u", (unsigned)(magnitude % 1000000u));
        return quanta_copy(out, capacity, text);
    case QUANTA_BOOL: return quanta_copy(out, capacity, value->as.boolean ? "true" : "false");
    case QUANTA_STRING:
        if (!memchr(value->as.string, 0, sizeof(value->as.string))) return 0;
        return quanta_copy(out, capacity, value->as.string);
    default: return 0;
    }
}
