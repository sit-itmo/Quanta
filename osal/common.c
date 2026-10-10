/* SPDX-License-Identifier: MIT */
#include "osal_internal.h"
#include <ctype.h>
#include <math.h>
#include <time.h>

quanta_status quanta_errno_status(int e)
{
    if (e == EACCES || e == EPERM)
    {
        return QUANTA_PERMISSION_DENIED;
    }
    if (e == ENOENT || e == ENODEV || e == ENXIO)
    {
        return QUANTA_NOT_PRESENT;
    }
    if (e == ENOMEM)
    {
        return QUANTA_NO_MEMORY;
    }
#ifdef ENODATA
    if (e == ENODATA)
    {
        return QUANTA_NO_DATA;
    }
#endif
#ifdef ETIMEDOUT
    if (e == ETIMEDOUT)
    {
        return QUANTA_TIMEOUT;
    }
#endif
#ifdef EOPNOTSUPP
    if (e == EOPNOTSUPP)
    {
        return QUANTA_UNSUPPORTED;
    }
#endif
    return QUANTA_IO_ERROR;
}

void quanta_utc_string(uint64_t ns, char *out, size_t cap)
{
    time_t t = (time_t)(ns / QUANTA_NS_S);
    struct tm tm;
    char b[32];
#ifdef _WIN32
    if (gmtime_s(&tm, &t) != 0)
    {
#else
    if (!gmtime_r(&t, &tm))
    {
#endif
        quanta_copy(out, cap, "invalid-time");
        return;
    }
    strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%S", &tm);
    snprintf(out, cap, "%s.%03uZ", b, (unsigned)((ns % QUANTA_NS_S) / QUANTA_NS_MS));
}
