/* SPDX-License-Identifier: MIT */
#include "os.h"
#include "quanta_hal_internal.h"
#include <ctype.h>
#include <math.h>
#include <sys/utsname.h>
#include <unistd.h>

/* Parse one whitespace-delimited nonnegative finite number, without accepting
 * junk suffixes. */
static int parse_number(const char **cursor, double *value)
{
    char *end;
    const char *text = *cursor;
    while (isspace((unsigned char)*text))
    {
        ++text;
    }
    if (!isdigit((unsigned char)*text))
    {
        return 0;
    }
    errno = 0;
    *value = strtod(text, &end);
    if (errno || end == text || !isfinite(*value) || *value < 0 ||
        (*end && !isspace((unsigned char)*end)))
    {
        return 0;
    }
    *cursor = end;
    return 1;
}

/* os-release is data, never a shell script. No expansion or external process is
 * used. */
static void pretty_name(const quanta_hal_context *hal, char *name, size_t capacity)
{
    char path[QUANTA_PATH_CAP], text[16384];
    const char *line;
    if (!quanta_path(path, sizeof(path), hal->paths.etc_root, "os-release") ||
        quanta_read_text(path, text, sizeof(text)) != QUANTA_OK)
    {
        return;
    }
    for (line = text; *line;)
    {
        const char *end = strchr(line, '\n');
        const char *p;
        char value[QUANTA_TEXT_CAP], quote = 0;
        size_t used = 0;
        if (!end)
        {
            end = line + strlen(line);
        }
        if ((size_t)(end - line) >= 12 && !memcmp(line, "PRETTY_NAME=", 12))
        {
            p = line + 12;
            if (p < end && (*p == '\'' || *p == '"'))
            {
                quote = *p++;
            }
            while (p < end && (!quote || *p != quote))
            {
                char ch = *p++;
                if (quote == '"' && ch == '\\' && p < end &&
                    (*p == '"' || *p == '\\' || *p == '$' || *p == '`'))
                {
                    ch = *p++;
                }
                if (used + 1 >= sizeof(value))
                {
                    return;
                }
                value[used++] = ch;
            }
            if (quote && (p == end || *p++ != quote))
            {
                return;
            }
            while (p < end && isspace((unsigned char)*p))
            {
                ++p;
            }
            if (p != end)
            {
                return;
            }
            value[used] = 0;
            quanta_trim(value);
            if (value[0])
            {
                (void)quanta_copy(name, capacity, value);
            }
            return;
        }
        line = *end ? end + 1 : end;
    }
}

quanta_status quanta_hal_os_refresh(quanta_os_snapshot *out, quanta_hal_context context)
{
    const quanta_hal_context *hal = &context;
    struct utsname identity;
    char path[QUANTA_PATH_CAP], buffer[512];
    const char *cursor;
    long online;
    size_t i;
    memset(out, 0, sizeof(*out));
    if (gethostname(out->hostname, sizeof(out->hostname)) != 0)
    {
        out->hostname_status = quanta_errno_status(errno);
    }
    else if (!memchr(out->hostname, 0, sizeof(out->hostname)))
    {
        out->hostname_status = QUANTA_LIMIT;
    }
    if (uname(&identity) != 0)
    {
        out->kernel_status = quanta_errno_status(errno);
    }
    else
    {
        if (!quanta_copy(out->kernel, sizeof(out->kernel), identity.release) ||
            !quanta_copy(out->version, sizeof(out->version), identity.version) ||
            !quanta_copy(out->arch, sizeof(out->arch), identity.machine) ||
            !quanta_copy(out->name, sizeof(out->name), identity.sysname))
        {
            out->kernel_status = QUANTA_LIMIT;
        }
        pretty_name(hal, out->name, sizeof(out->name));
    }
    online = sysconf(_SC_NPROCESSORS_ONLN);
    out->online_status = online > 0 ? QUANTA_OK : QUANTA_NO_DATA;
    if (online > 0)
    {
        out->online_cpus = (uint64_t)online;
    }
    if (!quanta_path(path, sizeof(path), hal->paths.proc_root, "uptime"))
    {
        out->uptime_status = QUANTA_LIMIT;
    }
    else
    {
        out->uptime_status = quanta_read_text(path, buffer, sizeof(buffer));
        cursor = buffer;
        if (out->uptime_status == QUANTA_OK && !parse_number(&cursor, &out->uptime))
        {
            out->uptime_status = QUANTA_INVALID_DATA;
        }
    }
    if (!quanta_path(path, sizeof(path), hal->paths.proc_root, "loadavg"))
    {
        out->load_status = QUANTA_LIMIT;
    }
    else
    {
        out->load_status = quanta_read_text(path, buffer, sizeof(buffer));
        cursor = buffer;
        if (out->load_status == QUANTA_OK)
        {
            for (i = 0; i < 3; ++i)
            {
                if (!parse_number(&cursor, &out->load[i]))
                {
                    out->load_status = QUANTA_INVALID_DATA;
                    break;
                }
            }
        }
    }
    return QUANTA_OK;
}

void quanta_hal_os_cleanup(quanta_os_snapshot *data)
{
    (void)data;
}
