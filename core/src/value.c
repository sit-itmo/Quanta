/* SPDX-License-Identifier: MIT */
#include "core_internal.h"
#include <ctype.h>
#include <math.h>
#include <time.h>

int quanta_value_number(const quanta_value *v, double *n)
{
    if (!v || !n || quanta_value_validate(v) != QUANTA_OK)
    {
        return 0;
    }
    switch (v->type)
    {
        case QUANTA_U64:
            *n = (double)v->as.u64;
            break;
        case QUANTA_S64:
            *n = (double)v->as.s64;
            break;
        case QUANTA_F64:
            *n = v->as.f64;
            break;
        case QUANTA_BOOL:
            *n = v->as.boolean ? 1 : 0;
            break;
        default:
            return 0;
    }
    return isfinite(*n);
}

int quanta_unit_scale(const char *c, const char *d, double *s)
{
    if (!c || !d || !s)
    {
        return 0;
    }
    *s = 1;
    if (!*d || !strcmp(c, d))
    {
        return 1;
    }
    if (!strcmp(c, "1") && !strcmp(d, "%"))
    {
        *s = 100;
        return 1;
    }
    if (!strcmp(c, "B") || !strcmp(c, "B/s"))
    {
        int rate = !strcmp(c, "B/s");
        const char *u[] = {"KiB", "MiB", "GiB", "TiB", "KiB/s", "MiB/s", "GiB/s", "TiB/s"};
        size_t i;
        for (i = 0; i < 4; i++)
        {
            if (!strcmp(d, u[i + (rate ? 4 : 0)]))
            {
                *s = 1.0 / pow(1024.0, (double)i + 1);
                return 1;
            }
        }
    }
    if (!strcmp(c, "Hz"))
    {
        if (!strcmp(d, "kHz"))
        {
            *s = 1e-3;
            return 1;
        }
        if (!strcmp(d, "MHz"))
        {
            *s = 1e-6;
            return 1;
        }
        if (!strcmp(d, "GHz"))
        {
            *s = 1e-9;
            return 1;
        }
    }
    if (!strcmp(c, "s") && !strcmp(d, "ms"))
    {
        *s = 1000;
        return 1;
    }
    return 0;
}

quanta_status quanta_counter_derive(quanta_transform op, const quanta_sample *prev,
                                    const quanta_sample *cur, quanta_value *v, uint64_t *window)
{
    uint64_t diff;
    if (!prev || !cur || !v || !window || (op != QUANTA_RATE && op != QUANTA_DELTA))
    {
        return QUANTA_INVALID_DATA;
    }
    memset(v, 0, sizeof(*v));
    v->type = op == QUANTA_RATE ? QUANTA_F64 : QUANTA_U64;
    *window = 0;
    if (cur->status != QUANTA_OK)
    {
        return cur->status;
    }
    if (prev->status != QUANTA_OK || prev->value.type != QUANTA_U64 ||
        cur->value.type != QUANTA_U64 || prev->counter_epoch != cur->counter_epoch ||
        cur->measured_at_ns <= prev->measured_at_ns || cur->value.as.u64 < prev->value.as.u64)
    {
        return QUANTA_WARMUP;
    }
    diff = cur->value.as.u64 - prev->value.as.u64;
    *window = cur->measured_at_ns - prev->measured_at_ns;
    if (op == QUANTA_DELTA)
    {
        v->type = QUANTA_U64;
        v->as.u64 = diff;
    }
    else
    {
        v->type = QUANTA_F64;
        v->as.f64 = (double)diff / ((double)*window / (double)QUANTA_NS_S);
    }
    return QUANTA_OK;
}

quanta_status quanta_value_validate(const quanta_value *value)
{
    if (!value || (unsigned)value->type > QUANTA_BYTES)
    {
        return QUANTA_INVALID_DATA;
    }
    switch (value->type)
    {
        case QUANTA_F64:
            return isfinite(value->as.f64) ? QUANTA_OK : QUANTA_INVALID_DATA;
        case QUANTA_BOOL:
            return value->as.boolean == 0 || value->as.boolean == 1 ? QUANTA_OK
                                                                    : QUANTA_INVALID_DATA;
        case QUANTA_STRING:
            return memchr(value->as.string, 0, sizeof(value->as.string)) ? QUANTA_OK
                                                                         : QUANTA_INVALID_DATA;
        case QUANTA_BYTES:
            return value->as.bytes.size <= sizeof(value->as.bytes.data) ? QUANTA_OK
                                                                        : QUANTA_INVALID_DATA;
        default:
            return QUANTA_OK;
    }
}
