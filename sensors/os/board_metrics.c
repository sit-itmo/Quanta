/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
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

quanta_status quanta_dmi_init(quanta_metric_context *c)
{
    c->private_data = quanta_calloc(1, sizeof(quanta_board_snapshot));
    return c->private_data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

quanta_status quanta_dmi_enumerate(quanta_metric_context *ctx, quanta_discovery_sink *sink)
{
    quanta_board_snapshot *data = (quanta_board_snapshot *)ctx->private_data;
    quanta_status status = quanta_native_dmi_refresh(data, quanta_sensor_snapshot_context(ctx));
    for (size_t i = 0; i < ctx->metric_count; i++)
    {
        const quanta_metric_desc *metric = ctx->metrics[i];
        int dimm;
        if (metric->init != quanta_dmi_init || !quanta_metric_wanted(ctx, metric))
        {
            continue;
        }
        dimm = !quanta_strncmp(metric->arg, "dimm_", 5);
        if (dimm && status != QUANTA_OK)
        {
            return status;
        }
        for (size_t j = 0; j < (dimm ? data->table.dimm_count : 1); j++)
        {
            quanta_instance instance;
            quanta_memset(&instance, 0, sizeof(instance));
            instance.metric = metric;
            if (dimm)
            {
                const quanta_dmi_dimm *slot = &data->table.dimms[j];
                char handle[24];
                quanta_snprintf(handle, sizeof(handle), "%u", (unsigned)slot->handle);
                quanta_copy(instance.resource, sizeof(instance.resource), handle);
                quanta_snprintf(instance.entity_key, sizeof(instance.entity_key), "dimm:%s", handle);
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

quanta_status quanta_dmi_collect(quanta_metric_context *c)
{
    return quanta_native_dmi_refresh((quanta_board_snapshot *)c->private_data, quanta_sensor_snapshot_context(c));
}

void quanta_dmi_destroy(quanta_metric_context *c)
{
    quanta_free(c->private_data);
    c->private_data = NULL;
}

quanta_status quanta_dmi_read(quanta_metric_context *c, quanta_instance *in, quanta_value *v, uint64_t *w,
                              uint32_t *q)
{
    quanta_board_snapshot *p = (quanta_board_snapshot *)c->private_data;
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
        if (!quanta_strcmp(a, fields[i]))
        {
            return copy_field(&p->table.fields[i], v);
        }
    }

    if (!quanta_strcmp(a, "system_uuid"))
    {
        return copy_field(&p->table.uuid, v);
    }

    if (quanta_strncmp(a, "dimm_", 5) || !quanta_parse_u64(in->resource, &handle))
    {
        return QUANTA_UNSUPPORTED;
    }

    for (i = 0; i < p->table.dimm_count; i++)
    {
        if (p->table.dimms[i].handle == handle)
        {
            quanta_dmi_dimm *d = &p->table.dimms[i];
            if (!quanta_strcmp(a, "dimm_size"))
            {
                v->as.u64 = d->size_bytes;
                return d->size_status;
            }
            if (!quanta_strcmp(a, "dimm_speed"))
            {
                v->as.u64 = d->speed_mt_s;
                return d->speed_status;
            }
            if (!quanta_strcmp(a, "dimm_vendor"))
            {
                return copy_field(&d->vendor, v);
            }
            if (!quanta_strcmp(a, "dimm_serial"))
            {
                return copy_field(&d->serial, v);
            }
            if (!quanta_strcmp(a, "dimm_part"))
            {
                return copy_field(&d->part, v);
            }
            return QUANTA_UNSUPPORTED;
        }
    }
    return QUANTA_GONE;
}

