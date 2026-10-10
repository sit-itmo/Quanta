/**
 * @file core/src/scheduler.c
 *
 * Polling loop: deadlines, shared snapshot refresh and delivery of completed frames and samples
 * to a sink.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Sample: the value of a particular series together with its timestamp, status and quality
 * flags. An error must not be presented as a valid zero.
 *
 * Sink: a set of engine notification callbacks that decouples data collection from file output,
 * TUI and GUI.
 */
/* SPDX-License-Identifier: MIT
 * One synchronous scheduler per engine. An acquisition is shared by every
 * due channel in a collection pass; bindings maintain independent transform
 * baselines.
 */
#include "quanta/osal.h"
#include "engine_internal.h"

uint64_t quanta_deadline_after(uint64_t now, uint64_t delay)
{
    return delay > UINT64_MAX - now ? UINT64_MAX : now + delay;
}

static quanta_status collect(quanta_engine *e, uint64_t now, int force, int *changed)
{
    unsigned refreshed[QUANTA_MAX_ACQUISITIONS] = {0};
    quanta_status status[QUANTA_MAX_ACQUISITIONS];
    size_t i, j;
    *changed = 0;
    quanta_memset(status, 0, sizeof(status));
    for (i = 0; i < e->channel_count; i++)
    {
        quanta_channel *c = &e->channels[i];
        const quanta_metric_desc *m = c->in.metric;
        quanta_metric_context *ctx;
        quanta_sample sample;
        size_t p;
        quanta_acquisition *a;
        uint64_t start;
        c->updated = 0;
        if (!c->active)
        {
            continue;
        }
        if (force && m->refresh == QUANTA_REFRESH_ONCE)
        {
            c->next = 0;
        }
        if (c->initialized && now < c->next)
        {
            continue;
        }
        p = c->acquisition_index;
        a = &e->acquisitions[p];
        ctx = &a->context;
        if (!refreshed[p])
        {
            ctx->now_ns = quanta_now_ns();
            ctx->error[0] = 0;
            status[p] = a->init_status != QUANTA_OK ? a->init_status
                        : m->collect         ? m->collect(ctx)
                                                       : QUANTA_OK;
            ctx->acquisition_ns = quanta_now_ns() - ctx->now_ns;
            ctx->now_ns += ctx->acquisition_ns;
            refreshed[p] = 1;
            a->refresh_count++;
        }
        quanta_memset(&sample, 0, sizeof(sample));
        sample.series_id = c->in.series_id;
        sample.sequence = c->last.sequence + 1;
        sample.value.type = m->type;
        sample.counter_epoch = c->in.generation;
        start = quanta_now_ns();
        sample.status = status[p];
        if (sample.status == QUANTA_OK)
        {
            sample.status =
                m->read(ctx, &c->in, &sample.value, &sample.window_ns, &sample.quality_flags);
        }
        sample.measured_at_ns = quanta_now_ns();
        sample.read_duration_ns =
            sample.measured_at_ns - start + (m->collect ? ctx->acquisition_ns : 0);
        /* Group-backed fields use the same acquisition time. */
        if (m->collect)
        {
            sample.measured_at_ns = ctx->now_ns;
        }
        if ((unsigned)sample.status > QUANTA_BAD_QUERY)
        {
            sample.status = QUANTA_INVALID_DATA;
        }
        if (sample.status == QUANTA_OK)
        {
            if (sample.value.type != m->type || quanta_value_validate(&sample.value) != QUANTA_OK)
            {
                sample.status = QUANTA_INVALID_DATA;
            }
        }
        if (sample.status != QUANTA_OK)
        {
            quanta_copy(sample.message, sizeof(sample.message),
                        ctx->error[0] ? ctx->error : quanta_status_name(sample.status));
            if (sample.status != QUANTA_WARMUP)
            {
                e->has_errors = 1;
            }
        }
        c->last = sample;
        c->initialized = 1;
        c->updated = 1;
        c->next = m->refresh == QUANTA_REFRESH_ONCE
                      ? UINT64_MAX
                      : quanta_deadline_after(quanta_now_ns(), c->period);
    }
    for (j = 0; j < e->binding_count; j++)
    {
        quanta_binding *b = &e->bindings[j];
        quanta_channel *c;
        quanta_sample result;
        if (!b->active)
        {
            continue;
        }
        c = &e->channels[b->channel_index];
        if (!c->updated || (b->has_last && now < b->next &&
                            !(force && c->in.metric->refresh == QUANTA_REFRESH_ONCE)))
        {
            continue;
        }
        result = c->last;
        result.series_id = b->meta.id;
        result.sequence = b->has_last ? b->last.sequence + 1 : 1;
        if (b->meta.transform != QUANTA_RAW)
        {
            result.status = quanta_counter_derive(b->meta.transform, &b->previous, &c->last,
                                                  &result.value, &result.window_ns);
            b->previous = c->last;
            result.value.type = b->meta.type;
            if (result.status != QUANTA_OK)
            {
                quanta_copy(result.message, sizeof(result.message),
                            quanta_status_name(result.status));
            }
        }
        if (result.status != QUANTA_OK && result.status != QUANTA_WARMUP)
        {
            e->has_errors = 1;
        }
        b->last = result;
        b->has_last = 1;
        b->dirty = 1;
        b->next = quanta_deadline_after(now, b->meta.effective_period_ns);
        *changed = 1;
    }
    return QUANTA_OK;
}

static quanta_status emit_frame(quanta_engine *e, int all)
{
    quanta_frame_info f;
    size_t i;
    quanta_status s = QUANTA_OK;
    f.id = ++e->frame_id;
    f.monotonic_ns = quanta_now_ns();
    f.realtime_ns = quanta_realtime_ns();
    if (e->sink->begin_frame && (s = e->sink->begin_frame(e->sink->context, &f)) != QUANTA_OK)
    {
        return s;
    }
    for (i = 0; i < e->binding_count; i++)
    {
        if (e->bindings[i].active && e->bindings[i].has_last && (all || e->bindings[i].dirty))
        {
            if (e->sink->emit_sample &&
                (s = e->sink->emit_sample(e->sink->context, &e->bindings[i].last)) != QUANTA_OK)
            {
                return s;
            }
            e->bindings[i].dirty = 0;
        }
    }
    return e->sink->end_frame ? e->sink->end_frame(e->sink->context) : QUANTA_OK;
}

quanta_status quanta_engine_run(quanta_engine *e, quanta_sink *sink)
{
    quanta_status result = QUANTA_OK;
    uint64_t start = quanta_now_ns(),
             next_discovery = quanta_deadline_after(start, 5 * QUANTA_NS_S), next_heartbeat = start;
    uint64_t warm_until;
    size_t i, j;
    int changed;
    if (!e || !sink)
    {
        return QUANTA_INVALID_DATA;
    }
    if (e->has_run)
    {
        return QUANTA_INVALID_DATA;
    }
    warm_until = e->plan.once ? quanta_deadline_after(start, e->plan.warmup_ns) : 0;
    e->sink = sink;
    if (sink->api_version != 1 || sink->struct_size < sizeof(*sink))
    {
        e->sink = NULL;
        return QUANTA_UNSUPPORTED;
    }
    e->has_run = 1;
    if (sink->begin_session && (result = sink->begin_session(sink->context, &e->plan)) != QUANTA_OK)
    {
        goto end;
    }
    for (i = 0; i < e->binding_count; i++)
    {
        if (e->bindings[i].active &&
            (result = quanta_engine_announce(e, &e->bindings[i])) != QUANTA_OK)
        {
            goto end;
        }
    }
    for (i = 0; i < e->plan.select_count; i++)
    {
        int found = 0;
        for (j = 0; j < e->binding_count; j++)
        {
            if (e->bindings[j].active && e->bindings[j].meta.selection_index == i)
            {
                found = 1;
            }
        }
        if (!found)
        {
            char msg[256];
            quanta_snprintf(msg, sizeof(msg), "optional selector '%s' has no available instances",
                     e->plan.selects[i].pattern);
            if ((result = quanta_engine_diagnostic(e, QUANTA_NOT_PRESENT, msg)) != QUANTA_OK)
            {
                goto end;
            }
        }
    }
    for (i = 0; i < e->binding_count; i++)
    {
        if (e->bindings[i].active)
        {
            quanta_binding *b = &e->bindings[i];
            uint64_t requested = e->plan.selects[b->meta.selection_index].period_ns;
            if (b->meta.metric->refresh == QUANTA_REFRESH_POLL &&
                b->meta.effective_period_ns > requested)
            {
                char msg[256];
                quanta_snprintf(msg, sizeof(msg), "%s: polling clamped from %.3fs to %.3fs", b->meta.key,
                         (double)requested / 1e9, (double)b->meta.effective_period_ns / 1e9);
                if ((result = quanta_engine_diagnostic(e, QUANTA_OK, msg)) != QUANTA_OK)
                {
                    goto end;
                }
            }
        }
    }
    while (!quanta_engine_stopping(e))
    {
        uint64_t now = quanta_now_ns(), next = quanta_deadline_after(now, 50 * QUANTA_NS_MS);
        int force = quanta_engine_consume_refresh(e);
        if (e->plan.duration_ns && now - start >= e->plan.duration_ns)
        {
            break;
        }
        if (now >= next_discovery || force)
        {
            result = quanta_engine_discover(e);
            if (result != QUANTA_OK)
            {
                break;
            }
            next_discovery = quanta_deadline_after(now, 5 * QUANTA_NS_S);
        }
        result = collect(e, now, force, &changed);
        if (result != QUANTA_OK)
        {
            break;
        }
        if (e->plan.once)
        {
            if (now >= warm_until)
            {
                result = emit_frame(e, 1);
                break;
            }
        }
        else if (changed || now >= next_heartbeat)
        {
            result = emit_frame(e, 0);
            if (result != QUANTA_OK)
            {
                break;
            }
            next_heartbeat = quanta_deadline_after(now, e->plan.period_ns);
            if (e->plan.count && e->frame_id >= e->plan.count)
            {
                break;
            }
        }
        for (i = 0; i < e->channel_count; i++)
        {
            if (e->channels[i].active && e->channels[i].next < next)
            {
                next = e->channels[i].next;
            }
        }
        if (e->plan.once && warm_until < next)
        {
            next = warm_until;
        }
        now = quanta_now_ns();
        if (next > now)
        {
            quanta_sleep_ns(next - now);
        }
    }
    if (quanta_engine_stopping(e) && result == QUANTA_OK)
    {
        result = QUANTA_CANCELLED;
    }
end:
    if (sink->end_session)
    {
        quanta_status s = sink->end_session(sink->context, result);
        if (result == QUANTA_OK && s != QUANTA_OK)
        {
            result = s;
        }
    }
    e->sink = NULL;
    return result;
}
