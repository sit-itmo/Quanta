/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "normalize.h"
#include <limits.h>
#include <math.h>
#include <string.h>

quanta_status quanta_memory_used(uint64_t total, uint64_t available, uint64_t *used,
                                     double *fraction)
{
    if (!total || available > total)
    {
        return QUANTA_INVALID_DATA;
    }
    *used = total - available;
    *fraction = (double)*used / (double)total;
    return QUANTA_OK;
}

quanta_status quanta_cpu_busy_fraction(uint64_t pt, uint64_t pi, uint64_t total, uint64_t idle,
                                      double *out)
{
    uint64_t dt, di;
    if (idle > total || pi > pt)
    {
        return QUANTA_INVALID_DATA;
    }
    if (total <= pt || idle < pi)
    {
        return QUANTA_WARMUP;
    }
    dt = total - pt;
    di = idle - pi;
    if (di > dt)
    {
        return QUANTA_WARMUP;
    }
    *out = (double)(dt - di) / (double)dt;
    return QUANTA_OK;
}

quanta_status quanta_pages_bytes(uint64_t pages, uint64_t size, uint64_t *out)
{
    if (!size || pages > UINT64_MAX / size)
    {
        return QUANTA_INVALID_DATA;
    }
    *out = pages * size;
    return QUANTA_OK;
}

quanta_status quanta_kelvin_celsius(double kelvin, double *out)
{
    /* A zero/negative firmware reading is usually an unsupported channel.
       No fabricated CPU package association and no averaging of zones. */
    if (!quanta_isfinite(kelvin) || kelvin <= 0 || kelvin > 2000)
    {
        return QUANTA_INVALID_DATA;
    }
    *out = kelvin - 273.15;
    return QUANTA_OK;
}

