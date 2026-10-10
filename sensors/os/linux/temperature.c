// hal/linux/hwmon.c
#include "quanta/osal.h"
#include "hwmon.h"
#include "snapshot_internal.h"
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

quanta_status quanta_native_hwmon_open(quanta_hwmon_snapshot *out, quanta_snapshot_context context)
{
    (void)context;
    out->status = QUANTA_OK;
    return QUANTA_OK;
}

static int add_channel(quanta_hwmon_snapshot *out, const quanta_hwmon_channel *ch)
{
    if (out->count == out->cap)
    {
        size_t cap = out->cap ? out->cap * 2 : 16;
        quanta_hwmon_channel *p;
        if (cap > QUANTA_MAX_CHANNELS)
        {
            return 0;
        }
        p = (quanta_hwmon_channel *)quanta_realloc(out->ch, cap * sizeof(*p));
        if (!p)
        {
            return 0;
        }
        out->ch = p;
        out->cap = cap;
    }
    out->ch[out->count++] = *ch;
    return 1;
}

quanta_status quanta_native_hwmon_discover(quanta_hwmon_snapshot *out, quanta_snapshot_context context)
{
    const quanta_snapshot_context *c = &context;
    char root[QUANTA_PATH_CAP], path[QUANTA_PATH_CAP], base[QUANTA_PATH_CAP];
    char chip[QUANTA_TEXT_CAP], real[PATH_MAX];
    DIR *dir, *channels;
    struct dirent *de, *chd;
    quanta_status result = QUANTA_OK, incomplete = QUANTA_OK;
    out->count = 0;
    out->status = QUANTA_OK;
    if (!quanta_path(root, sizeof(root), c->paths.sys_root, "class/hwmon"))
    {
        return QUANTA_LIMIT;
    }
    dir = opendir(root);
    if (!dir)
    {
        return quanta_errno_status(quanta_errno);
    }
    while ((de = readdir(dir)) != NULL)
    {
        struct stat ds;
        uint64_t interval_ms = 0;
        if (quanta_strncmp(de->d_name, "hwmon", 5))
        {
            continue;
        }
        if (!quanta_path(base, sizeof(base), root, de->d_name) ||
            !quanta_path(path, sizeof(path), base, "name"))
        {
            result = QUANTA_LIMIT;
            break;
        }
        if (quanta_read_text(path, chip, sizeof(chip)) != QUANTA_OK)
        {
            quanta_copy(chip, sizeof(chip), de->d_name);
        }
        quanta_trim(chip);
        if (!realpath(base, real) || stat(base, &ds))
        {
            incomplete = quanta_errno_status(quanta_errno);
            continue;
        }
        quanta_path(path, sizeof(path), base, "update_interval");
        if (quanta_read_u64(path, &interval_ms) != QUANTA_OK || interval_ms > 604800000)
        {
            interval_ms = 0;
        }
        channels = opendir(base);
        if (!channels)
        {
            incomplete = quanta_errno_status(quanta_errno);
            continue;
        }
        while ((chd = readdir(channels)) != NULL)
        {
            quanta_hwmon_channel chan;
            char label[QUANTA_TEXT_CAP], attr[80];
            const char *num;
            size_t plen;
            if (!quanta_strncmp(chd->d_name, "temp", 4))
            {
                plen = 4;
                chan.kind = HWMON_TEMP;
                chan.unit = HWMON_MILLI_C;
            }
            else if (!quanta_strncmp(chd->d_name, "fan", 3))
            {
                plen = 3;
                chan.kind = HWMON_FAN;
                chan.unit = HWMON_RPM;
            }
            else
            {
                continue;
            }
            num = chd->d_name + plen;
            if (!quanta_isdigit((unsigned char)*num))
            {
                continue;
            }
            while (quanta_isdigit((unsigned char)*num))
            {
                num++;
            }
            if (quanta_strcmp(num, "_input"))
            {
                continue;
            }
            if ((size_t)(num - chd->d_name) >= sizeof(chan.channel))
            {
                result = QUANTA_LIMIT;
                break;
            }
            quanta_memset(&chan, 0, sizeof(chan));
            if (!quanta_strncmp(chd->d_name, "temp", 4))
            {
                chan.kind = HWMON_TEMP;
                chan.unit = HWMON_MILLI_C;
            }
            else
            {
                chan.kind = HWMON_FAN;
                chan.unit = HWMON_RPM;
            }
            quanta_memcpy(chan.channel, chd->d_name, (size_t)(num - chd->d_name));
            chan.channel[num - chd->d_name] = 0;
            quanta_copy(chan.chip, sizeof(chan.chip), chip);
            quanta_snprintf(attr, sizeof(attr), "%s_label", chan.channel);
            quanta_path(path, sizeof(path), base, attr);
            if (quanta_read_text(path, label, sizeof(label)) != QUANTA_OK)
            {
                quanta_copy(label, sizeof(label), chan.channel);
            }
            quanta_trim(label);
            quanta_copy(chan.label, sizeof(chan.label), label);
            quanta_copy(chan.device, sizeof(chan.device), real);
            chan.min_poll_ns = interval_ms * QUANTA_NS_MS;
            if (quanta_snprintf(chan.entity_key, sizeof(chan.entity_key), "%s:%" PRIuMAX ":%s", real,
                         (uintmax_t)ds.st_ino, chan.channel) >= (int)sizeof(chan.entity_key) ||
                !quanta_path(chan.resource, sizeof(chan.resource), base, chd->d_name))
            {
                result = QUANTA_LIMIT;
                break;
            }
            if (!add_channel(out, &chan))
            {
                result = QUANTA_NO_MEMORY;
                break;
            }
        }
        closedir(channels);
        if (result != QUANTA_OK)
        {
            break;
        }
    }
    closedir(dir);
    out->status = result == QUANTA_OK ? incomplete : result;
    return out->status;
}

quanta_status quanta_native_hwmon_refresh(quanta_hwmon_snapshot *out, quanta_snapshot_context context)
{
    (void)out;
    (void)context;
    return QUANTA_OK; /* Linux reads each channel per sample; no group snapshot.
                       */
}

quanta_status quanta_native_hwmon_read(const quanta_hwmon_snapshot *d, const quanta_hwmon_channel *ch,
                                    double *raw, quanta_status *fault)
{
    char b[128], fp[QUANTA_PATH_CAP];
    char *end;
    uint64_t f;
    int64_t x;
    quanta_status s;
    (void)d;
    *fault = QUANTA_OK;
    quanta_copy(fp, sizeof(fp), ch->resource);
    end = quanta_strrchr(fp, '_');
    if (end)
    {
        quanta_copy(end, sizeof(fp) - (size_t)(end - fp), "_fault");
    }
    s = quanta_read_u64(fp, &f);
    if (s == QUANTA_OK && f)
    {
        *fault = QUANTA_NO_DATA;
        return QUANTA_OK;
    }
    if (s != QUANTA_OK && s != QUANTA_NOT_PRESENT)
    {
        return s;
    }
    s = quanta_read_text(ch->resource, b, sizeof(b));
    if (s != QUANTA_OK)
    {
        return s;
    }
    if (!quanta_parse_s64(b, &x))
    {
        return QUANTA_INVALID_DATA;
    }
    *raw = (double)x;
    return QUANTA_OK;
}

void quanta_native_hwmon_cleanup(quanta_hwmon_snapshot *d)
{
    quanta_free(d->ch);
    d->ch = NULL;
    d->count = d->cap = 0;
}
