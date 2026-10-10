#include "quanta/osal.h"
#include "collector_internal.h"
#include "network.h"

quanta_status quanta_network_init(quanta_metric_context *c)
{
    c->private_data = quanta_calloc(1, sizeof(quanta_network_snapshot));
    return c->private_data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

quanta_status quanta_network_collect(quanta_metric_context *c)
{
    return quanta_native_network_refresh((quanta_network_snapshot *)c->private_data,
                                      quanta_sensor_snapshot_context(c));
}

void quanta_network_destroy(quanta_metric_context *c)
{
    if (!c->private_data)
    {
        return;
    }
    quanta_native_network_cleanup((quanta_network_snapshot *)c->private_data);
    quanta_free(c->private_data);
    c->private_data = NULL;
}

quanta_status quanta_network_enumerate(quanta_metric_context *c, quanta_discovery_sink *sink)
{
    quanta_network_snapshot *d = (quanta_network_snapshot *)c->private_data;
    const quanta_metric_desc *const *r;
    size_t n, i, j;
    quanta_status s = quanta_native_network_refresh(d, quanta_sensor_snapshot_context(c));
    if (s != QUANTA_OK)
    {
        return s;
    }
    r = (n = c->metric_count, c->metrics);
    for (i = 0; i < d->count; i++)
    {
        const quanta_network_row *row = &d->rows[i];
        for (j = 0; j < n; j++)
        {
            quanta_instance in;
            quanta_status st;
            if (r[j]->init != quanta_network_init || !quanta_metric_wanted(c, r[j]))
            {
                continue;
            }
            quanta_memset(&in, 0, sizeof(in));
            in.metric = r[j];
            quanta_copy(in.entity_key, sizeof(in.entity_key), row->entity_key);
            if (!quanta_label_set(in.labels, &in.label_count, "ifname", row->name))
            {
                return QUANTA_LIMIT;
            }
            {
                char index[32];
                quanta_snprintf(index, sizeof(index), "%" PRIu64, row->ifindex);
                if (!quanta_label_set(in.labels, &in.label_count, "ifindex", index))
                {
                    return QUANTA_LIMIT;
                }
                if (row->guid[0] &&
                    !quanta_label_set(in.labels, &in.label_count, "guid", row->guid))
                {
                    return QUANTA_LIMIT;
                }
            }
            st = sink->emit(sink->context, &in);
            if (st != QUANTA_OK)
            {
                return st;
            }
        }
    }
    return QUANTA_OK;
}

static const quanta_network_row *find_row(const quanta_network_snapshot *d, const char *name)
{
    size_t i;
    for (i = 0; i < d->count; i++)
    {
        if (!quanta_strcmp(d->rows[i].entity_key, name))
        {
            return &d->rows[i];
        }
    }
    return NULL;
}

quanta_status quanta_network_read(quanta_metric_context *c, quanta_instance *in, quanta_value *v,
                                  uint64_t *w, uint32_t *q)
{
    const quanta_network_snapshot *d = (const quanta_network_snapshot *)c->private_data;
    const quanta_network_row *r = find_row(d, in->entity_key);
    const char *a = in->metric->arg;
    (void)w;
    (void)q;
    if (!r)
    {
        return QUANTA_GONE;
    }
    if (!quanta_strcmp(a, "address"))
    {
        if (r->address_status != QUANTA_OK)
        {
            return r->address_status;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), r->address) ? QUANTA_OK
                                                                           : QUANTA_LIMIT;
    }
    if (!quanta_strcmp(a, "operstate"))
    {
        if (r->state_status != QUANTA_OK)
        {
            return r->state_status;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), r->state) ? QUANTA_OK : QUANTA_LIMIT;
    }
    if (!quanta_strcmp(a, "rx_link"))
    {
        v->as.u64 = r->rx_link_bps;
        return r->link_status;
    }
    if (!quanta_strcmp(a, "tx_link"))
    {
        v->as.u64 = r->tx_link_bps;
        return r->link_status;
    }
    if (r->counters_status != QUANTA_OK)
    {
        return r->counters_status;
    }
    if (!quanta_strcmp(a, "rx_bytes"))
    {
        v->as.u64 = r->rx_bytes;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "rx_packets"))
    {
        v->as.u64 = r->rx_packets;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "rx_errors"))
    {
        v->as.u64 = r->rx_errors;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "tx_bytes"))
    {
        v->as.u64 = r->tx_bytes;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "tx_packets"))
    {
        v->as.u64 = r->tx_packets;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "tx_errors"))
    {
        v->as.u64 = r->tx_errors;
        return QUANTA_OK;
    }
    return QUANTA_UNSUPPORTED;
}

