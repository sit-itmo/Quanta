/* SPDX-License-Identifier: MIT
 * Bounded, lossless event queue. Backpressure delays acquisition rather than
 * inventing samples or dropping metadata. Only the UI thread calls its sink.
 */
#include "quanta/osal.h"
#include "quanta/live.h"
#include "core_internal.h"
#ifndef QUANTA_LIVE_QUEUE_CAP
#define QUANTA_LIVE_QUEUE_CAP 256
#endif
#define QUEUE_CAP QUANTA_LIVE_QUEUE_CAP

typedef enum event_kind
{
    E_BEGIN,
    E_UPSERT,
    E_REMOVE,
    E_FRAME,
    E_SAMPLE,
    E_END_FRAME,
    E_DIAG,
    E_PROGRESS,
    E_END
} event_kind;

typedef struct event
{
    event_kind kind;

    union
    {
        quanta_series series;
        quanta_sample sample;
        quanta_frame_info frame;
        uint64_t id;

        struct
        {
            quanta_status status;
            char text[256];
            uint32_t index, total;
        } diag;
    } u;
} event;

struct quanta_live
{
    quanta_engine *engine;
    const quanta_plan *plan;
    quanta_thread thread;
    quanta_mutex mutex;
    quanta_condition space;
    event *events;
    size_t head, count;
    int closed, done;
    quanta_status result;
};

static quanta_status push(quanta_live *l, const event *e)
{
    quanta_mutex_lock(&l->mutex);
    while (l->count == QUEUE_CAP && !l->closed)
    {
        quanta_condition_wait(&l->space, &l->mutex);
    }
    if (l->closed)
    {
        quanta_mutex_unlock(&l->mutex);
        return QUANTA_CANCELLED;
    }
    l->events[(l->head + l->count) % QUEUE_CAP] = *e;
    l->count++;
    quanta_mutex_unlock(&l->mutex);
    return QUANTA_OK;
}

static quanta_status qbegin(void *c, const quanta_plan *p)
{
    event e;
    quanta_memset(&e, 0, sizeof(e));
    (void)p;
    e.kind = E_BEGIN;
    return push((quanta_live *)c, &e);
}

static quanta_status qupsert(void *c, const quanta_series *s)
{
    event e;
    quanta_memset(&e, 0, sizeof(e));
    e.kind = E_UPSERT;
    e.u.series = *s;
    return push((quanta_live *)c, &e);
}

static quanta_status qremove(void *c, uint64_t id)
{
    event e;
    quanta_memset(&e, 0, sizeof(e));
    e.kind = E_REMOVE;
    e.u.id = id;
    return push((quanta_live *)c, &e);
}

static quanta_status qframe(void *c, const quanta_frame_info *f)
{
    event e;
    quanta_memset(&e, 0, sizeof(e));
    e.kind = E_FRAME;
    e.u.frame = *f;
    return push((quanta_live *)c, &e);
}

static quanta_status qsample(void *c, const quanta_sample *s)
{
    event e;
    quanta_memset(&e, 0, sizeof(e));
    e.kind = E_SAMPLE;
    e.u.sample = *s;
    return push((quanta_live *)c, &e);
}

static quanta_status qendframe(void *c)
{
    event e;
    quanta_memset(&e, 0, sizeof(e));
    e.kind = E_END_FRAME;
    return push((quanta_live *)c, &e);
}

static quanta_status qdiag(void *c, quanta_status s, const char *text)
{
    event e;
    quanta_memset(&e, 0, sizeof(e));
    e.kind = E_DIAG;
    e.u.diag.status = s;
    quanta_safe_text(e.u.diag.text, sizeof(e.u.diag.text), text);
    return push((quanta_live *)c, &e);
}

static quanta_status qprogress(void *c, uint32_t i, uint32_t n, const char *text)
{
    event e;
    quanta_memset(&e, 0, sizeof(e));
    e.kind = E_PROGRESS;
    e.u.diag.index = i;
    e.u.diag.total = n;
    quanta_safe_text(e.u.diag.text, sizeof(e.u.diag.text), text);
    return push((quanta_live *)c, &e);
}

static quanta_status qend(void *c, quanta_status s)
{
    event e;
    quanta_memset(&e, 0, sizeof(e));
    e.kind = E_END;
    e.u.diag.status = s;
    return push((quanta_live *)c, &e);
}

static void *worker(void *p)
{
    quanta_live *l = (quanta_live *)p;
    quanta_status result;
    quanta_sink s;
    quanta_memset(&s, 0, sizeof(s));
    s.struct_size = sizeof(s);
    s.api_version = 1;
    s.context = l;
    s.begin_session = qbegin;
    s.upsert_series = qupsert;
    s.remove_series = qremove;
    s.begin_frame = qframe;
    s.emit_sample = qsample;
    s.end_frame = qendframe;
    s.emit_diagnostic = qdiag;
    s.emit_progress = qprogress;
    s.end_session = qend;
    result = quanta_engine_run(l->engine, &s);
    quanta_mutex_lock(&l->mutex);
    l->result = result;
    l->done = 1;
    quanta_mutex_unlock(&l->mutex);
    return NULL;
}

quanta_status quanta_live_start(quanta_engine *e, const quanta_plan *p, quanta_live **out)
{
    quanta_live *l;
    if (!out)
    {
        return QUANTA_INVALID_DATA;
    }
    *out = NULL;
    if (!e || !p)
    {
        return QUANTA_INVALID_DATA;
    }
    l = (quanta_live *)quanta_calloc(1, sizeof(*l));
    if (!l)
    {
        return QUANTA_NO_MEMORY;
    }
    l->engine = e;
    l->plan = p;
    l->events = (event *)quanta_calloc(QUEUE_CAP, sizeof(event));
    if (!l->events)
    {
        quanta_free(l);
        return QUANTA_NO_MEMORY;
    }
    if (quanta_mutex_init(&l->mutex))
    {
        quanta_free(l->events);
        quanta_free(l);
        return QUANTA_IO_ERROR;
    }
    if (quanta_condition_init(&l->space))
    {
        quanta_mutex_destroy(&l->mutex);
        quanta_free(l->events);
        quanta_free(l);
        return QUANTA_IO_ERROR;
    }
    if (quanta_thread_start(&l->thread, worker, l))
    {
        quanta_condition_destroy(&l->space);
        quanta_mutex_destroy(&l->mutex);
        quanta_free(l->events);
        quanta_free(l);
        return QUANTA_IO_ERROR;
    }
    *out = l;
    return QUANTA_OK;
}

quanta_status quanta_live_pump(quanta_live *l, quanta_sink *s)
{
    size_t processed = 0;
    if (!l || !s)
    {
        return QUANTA_INVALID_DATA;
    }
    if (s->api_version != 1 || s->struct_size < sizeof(*s))
    {
        return QUANTA_UNSUPPORTED;
    }
    while (processed++ < QUEUE_CAP * 4u)
    {
        event e;
        quanta_status st = QUANTA_OK;
        quanta_mutex_lock(&l->mutex);
        if (!l->count)
        {
            quanta_mutex_unlock(&l->mutex);
            break;
        }
        e = l->events[l->head];
        l->head = (l->head + 1) % QUEUE_CAP;
        l->count--;
        quanta_condition_signal(&l->space);
        quanta_mutex_unlock(&l->mutex);
        switch (e.kind)
        {
            case E_BEGIN:
                if (s->begin_session)
                {
                    st = s->begin_session(s->context, l->plan);
                }
                break;
            case E_UPSERT:
                if (s->upsert_series)
                {
                    st = s->upsert_series(s->context, &e.u.series);
                }
                break;
            case E_REMOVE:
                if (s->remove_series)
                {
                    st = s->remove_series(s->context, e.u.id);
                }
                break;
            case E_FRAME:
                if (s->begin_frame)
                {
                    st = s->begin_frame(s->context, &e.u.frame);
                }
                break;
            case E_SAMPLE:
                if (s->emit_sample)
                {
                    st = s->emit_sample(s->context, &e.u.sample);
                }
                break;
            case E_END_FRAME:
                if (s->end_frame)
                {
                    st = s->end_frame(s->context);
                }
                break;
            case E_DIAG:
                if (s->emit_diagnostic)
                {
                    st = s->emit_diagnostic(s->context, e.u.diag.status, e.u.diag.text);
                }
                break;
            case E_PROGRESS:
                if (s->emit_progress)
                {
                    st =
                        s->emit_progress(s->context, e.u.diag.index, e.u.diag.total, e.u.diag.text);
                }
                break;
            case E_END:
                if (s->end_session)
                {
                    st = s->end_session(s->context, e.u.diag.status);
                }
                break;
        }
        if (st != QUANTA_OK)
        {
            return st;
        }
    }
    return QUANTA_OK;
}

int quanta_live_done(quanta_live *l)
{
    int d;
    if (!l)
    {
        return 1;
    }
    quanta_mutex_lock(&l->mutex);
    d = l->done && !l->count;
    quanta_mutex_unlock(&l->mutex);
    return d;
}

quanta_status quanta_live_finish(quanta_live *l, int stop)
{
    quanta_status s;
    if (!l || (!stop && !quanta_live_done(l)))
    {
        return QUANTA_INVALID_DATA;
    }
    if (stop)
    {
        quanta_engine_stop(l->engine);
        quanta_mutex_lock(&l->mutex);
        l->closed = 1;
        quanta_condition_broadcast(&l->space);
        quanta_mutex_unlock(&l->mutex);
    }
    quanta_thread_join(&l->thread);
    s = l->result;
    quanta_condition_destroy(&l->space);
    quanta_mutex_destroy(&l->mutex);
    quanta_free(l->events);
    quanta_free(l);
    return s;
}
