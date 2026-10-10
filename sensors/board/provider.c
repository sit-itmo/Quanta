/* SPDX-License-Identifier: MIT */
// providers/dmi.c
#include "collector_internal.h"
#include "dmi.h"

static quanta_status copy_field(const quanta_dmi_string *s, quanta_value *v)
{
    if (s->status != QUANTA_OK)
    {
        return s->status;
    }
    return quanta_copy(v->as.string, sizeof(v->as.string), s->text) ? QUANTA_OK : QUANTA_LIMIT;
}

static quanta_status dmi_open(quanta_read_ctx *c)
{
    c->data = calloc(1, sizeof(quanta_board_snapshot));
    return c->data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

static quanta_status dmi_discover(quanta_read_ctx *ctx, quanta_discovery_sink *sink)
{
    quanta_board_snapshot *data = (quanta_board_snapshot *)ctx->data;
    quanta_status status = quanta_hal_dmi_refresh(data, quanta_sensor_hal_context(ctx));
    for (size_t i = 0; i < ctx->catalog->count; i++)
    {
        const quanta_metric_desc *metric = ctx->catalog->metrics[i];
        int dimm;
        if (metric->provider != &quanta_dmi_provider || !quanta_metric_wanted(ctx, metric))
        {
            continue;
        }
        dimm = !strncmp(metric->arg, "dimm_", 5);
        if (dimm && status != QUANTA_OK)
        {
            return status;
        }
        for (size_t j = 0; j < (dimm ? data->table.dimm_count : 1); j++)
        {
            quanta_instance instance;
            memset(&instance, 0, sizeof(instance));
            instance.metric = metric;
            if (dimm)
            {
                const quanta_dmi_dimm *slot = &data->table.dimms[j];
                char handle[24];
                snprintf(handle, sizeof(handle), "%u", (unsigned)slot->handle);
                quanta_copy(instance.resource, sizeof(instance.resource), handle);
                snprintf(instance.entity_key, sizeof(instance.entity_key), "dimm:%s", handle);
                quanta_label_set(instance.labels, &instance.label_count, "handle", handle);
                if (slot->locator.status == QUANTA_OK)
                {
                    quanta_label_set(instance.labels, &instance.label_count, "locator",
                                     slot->locator.text);
                }
                if (slot->bank.status == QUANTA_OK)
                {
                    quanta_label_set(instance.labels, &instance.label_count, "bank",
                                     slot->bank.text);
                }
            }
            else
            {
                quanta_copy(instance.entity_key, sizeof(instance.entity_key), "system");
            }
            quanta_status emitted = sink->emit(sink->context, &instance);
            if (emitted != QUANTA_OK)
            {
                return emitted;
            }
        }
    }
    return QUANTA_OK;
}

static quanta_status dmi_refresh(quanta_read_ctx *c)
{
    return quanta_hal_dmi_refresh((quanta_board_snapshot *)c->data, quanta_sensor_hal_context(c));
}

static void dmi_close(quanta_read_ctx *c)
{
    free(c->data);
    c->data = NULL;
}

quanta_status quanta_dmi_read(quanta_read_ctx *c, quanta_instance *in, quanta_value *v, uint64_t *w,
                              uint32_t *q)
{
    quanta_board_snapshot *p = (quanta_board_snapshot *)c->data;
    const char *a = in->metric->arg;
    static const char *const fields[] = {"bios_vendor",   "bios_version", "bios_date",
                                         "board_vendor",  "board_name",   "board_serial",
                                         "system_vendor", "system_model", "system_serial"};
    size_t i;
    uint64_t handle;
    (void)w;
    (void)q;

    if (p->status != QUANTA_OK)
    {
        return p->status;
    }

    for (i = 0; i < QUANTA_ARRAY_SIZE(fields); i++)
    {
        if (!strcmp(a, fields[i]))
        {
            return copy_field(&p->table.fields[i], v);
        }
    }

    if (!strcmp(a, "system_uuid"))
    {
        return copy_field(&p->table.uuid, v);
    }

    if (strncmp(a, "dimm_", 5) || !quanta_parse_u64(in->resource, &handle))
    {
        return QUANTA_UNSUPPORTED;
    }

    for (i = 0; i < p->table.dimm_count; i++)
    {
        if (p->table.dimms[i].handle == handle)
        {
            quanta_dmi_dimm *d = &p->table.dimms[i];
            if (!strcmp(a, "dimm_size"))
            {
                v->as.u64 = d->size_bytes;
                return d->size_status;
            }
            if (!strcmp(a, "dimm_speed"))
            {
                v->as.u64 = d->speed_mt_s;
                return d->speed_status;
            }
            if (!strcmp(a, "dimm_vendor"))
            {
                return copy_field(&d->vendor, v);
            }
            if (!strcmp(a, "dimm_serial"))
            {
                return copy_field(&d->serial, v);
            }
            if (!strcmp(a, "dimm_part"))
            {
                return copy_field(&d->part, v);
            }
            return QUANTA_UNSUPPORTED;
        }
    }
    return QUANTA_GONE;
}

const quanta_provider_desc quanta_dmi_provider = {
    3,
    "dmi",
    "Read-only DMI/SMBIOS firmware data (sysfs dmi-id / RSMB table); never "
    "elevates privileges",
    dmi_open,
    dmi_discover,
    dmi_refresh,
    dmi_close};
