/**
 * @file osal/common.c
 *
 * Shared file/text helpers built on the stdio contract and platform wrappers.
 *
 * OSAL: the minimal runtime contract for memory, files, clocks, threads and synchronization. OS
 * metrics belong to sensors, not OSAL.
 */
/* SPDX-License-Identifier: MIT */
#include "support.h"
quanta_status quanta_errno_status(int e)
{
#ifdef ENOSYS
    if (e == ENOSYS) return QUANTA_UNSUPPORTED;
#endif
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

int quanta_path(char *out, size_t capacity, const char *root, const char *relative)
{
    int length;
    if (!out || !capacity || !root || !relative) return 0;
    length = quanta_snprintf(out, capacity, "%s/%s", root, relative);
    return length >= 0 && (size_t)length < capacity;
}

quanta_status quanta_read_text(const char *path, char *out, size_t capacity)
{
    quanta_file *file;
    size_t used = 0;
    quanta_status status = QUANTA_OK;
    if (!out || !capacity) return QUANTA_INVALID_DATA;
    out[0] = 0;
    if (!path) return QUANTA_INVALID_DATA;
    if (capacity < 2) return QUANTA_LIMIT;
    file = quanta_fopen(path, "rb");
    if (!file) return quanta_errno_status(quanta_errno);
    while (used + 1 < capacity)
    {
        size_t count = quanta_fread(out + used, 1, capacity - 1 - used, file);
        used += count;
        if (quanta_ferror(file))
        {
            if (quanta_errno == EINTR)
            {
                quanta_clearerr(file);
                continue;
            }
            status = QUANTA_IO_ERROR;
            break;
        }
        if (!count || quanta_feof(file)) break;
    }
    if (status == QUANTA_OK && used + 1 == capacity)
    {
        int extra;
        do
        {
            quanta_errno = 0;
            extra = quanta_fgetc(file);
            if (extra == QUANTA_EOF && quanta_ferror(file) && quanta_errno == EINTR)
                quanta_clearerr(file);
            else break;
        } while (1);
        if (quanta_ferror(file)) status = QUANTA_IO_ERROR;
        else if (extra != QUANTA_EOF) status = QUANTA_LIMIT;
    }
    if (status == QUANTA_OK && quanta_memchr(out, 0, used)) status = QUANTA_INVALID_DATA;
    if (quanta_fclose(file) && status == QUANTA_OK) status = QUANTA_IO_ERROR;
    out[used] = 0;
    if (status != QUANTA_OK) out[0] = 0;
    return status;
}

quanta_status quanta_read_u64(const char *path, uint64_t *out)
{
    char text[128];
    quanta_status status;
    if (!out) return QUANTA_INVALID_DATA;
    *out = 0;
    status = quanta_read_text(path, text, sizeof(text));
    return status != QUANTA_OK ? status :
           quanta_parse_u64(text, out) ? QUANTA_OK : QUANTA_INVALID_DATA;
}

/* Positive Unix timestamps only. uint64 nanoseconds spans 1970..2554, so the
 * bounded year loop is small. No time_t, local timezone, gmtime_r or strftime. */
void quanta_utc_string(uint64_t nanoseconds, char *out, size_t capacity)
{
    static const unsigned month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint64_t seconds = nanoseconds / QUANTA_NS_S;
    uint64_t days = seconds / 86400;
    unsigned year = 1970, month = 0, leap;
    if (!out || !capacity) return;
#if QUANTA_PLATFORM_FREERTOS
    if (!nanoseconds) { quanta_copy(out, capacity, "unavailable"); return; }
#endif
    for (;;)
    {
        leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        if (days < 365u + leap) break;
        days -= 365u + leap;
        ++year;
    }
    while (month < 11)
    {
        unsigned length = month_days[month] + (month == 1 ? leap : 0);
        if (days < length) break;
        days -= length;
        ++month;
    }
    seconds %= 86400;
    quanta_snprintf(out, capacity, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
                    year, month + 1, (unsigned)days + 1,
                    (unsigned)(seconds / 3600), (unsigned)(seconds / 60 % 60),
                    (unsigned)(seconds % 60),
                    (unsigned)(nanoseconds % QUANTA_NS_S / QUANTA_NS_MS));
}
