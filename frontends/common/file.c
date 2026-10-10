/* SPDX-License-Identifier: MIT */
#include "frontend_internal.h"
#include "quanta/view.h"

typedef struct file_sink
{
    FILE *out;
    quanta_plan const *plan;
    quanta_model model;
    quanta_sink ms;
    char session[96];
} file_sink;

static void csv_cell(FILE *f, const char *text, int safe)
{
    const unsigned char *p = (const unsigned char *)text;
    fputc('"', f);
    if (safe && (*p == '=' || *p == '+' || *p == '-' || *p == '@' || *p == '\t' || *p == '\r'))
    {
        fputc('\'', f);
    }
    while (*p)
    {
        if (*p == '"')
        {
            fputc('"', f);
        }
        fputc(*p++, f);
    }
    fputc('"', f);
}

static int json_add(char *out, size_t cap, size_t *off, const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    if (*off + 1 >= cap)
    {
        return 0;
    }
    out[(*off)++] = '"';
    while (*p)
    {
        unsigned c = *p++;
        char b[8];
        const char *piece = b;
        size_t n;
        if (c == '"' || c == '\\')
        {
            b[0] = '\\';
            b[1] = (char)c;
            b[2] = 0;
        }
        else if (c < 32)
        {
            snprintf(b, sizeof(b), "\\u%04x", c);
        }
        else
        {
            b[0] = (char)c;
            b[1] = 0;
        }
        n = strlen(piece);
        if (*off + n + 2 >= cap)
        {
            return 0;
        }
        memcpy(out + *off, piece, n);
        *off += n;
    }
    out[(*off)++] = '"';
    out[*off] = 0;
    return 1;
}

static quanta_status labels_json(const quanta_series *s, char *out, size_t cap)
{
    size_t i, off = 0;
    if (cap < 3)
    {
        return QUANTA_LIMIT;
    }
    out[off++] = '{';
    for (i = 0; i < s->label_count; i++)
    {
        if (i)
        {
            out[off++] = ',';
        }
        if (!json_add(out, cap, &off, s->labels[i].key))
        {
            return QUANTA_LIMIT;
        }
        out[off++] = ':';
        if (!json_add(out, cap, &off, s->labels[i].value))
        {
            return QUANTA_LIMIT;
        }
    }
    if (off + 2 > cap)
    {
        return QUANTA_LIMIT;
    }
    out[off++] = '}';
    out[off] = 0;
    return QUANTA_OK;
}

static quanta_status write_csv(file_sink *f, const quanta_series *meta, const quanta_sample *s,
                               const char *event)
{
    char labels[32768], value[2048], time[64];
    int redact = f->plan->redact && (meta->metric->flags & QUANTA_SENSITIVE);
    quanta_status st = labels_json(meta, labels, sizeof(labels));
    uint64_t wall = f->model.frame.realtime_ns;
    if (st != QUANTA_OK)
    {
        return st;
    }
    if (s->measured_at_ns > f->model.frame.monotonic_ns)
    {
        wall = quanta_realtime_ns();
    }
    if (s->measured_at_ns <= f->model.frame.monotonic_ns)
    {
        uint64_t age = f->model.frame.monotonic_ns - s->measured_at_ns;
        if (age < wall)
        {
            wall -= age;
        }
    }
    quanta_utc_string(wall, time, sizeof(time));
    csv_cell(f->out, event, 0);
    fputc(',', f->out);
    csv_cell(f->out, f->session, 0);
    fprintf(f->out, ",%" PRIu64 ",%" PRIu64 ",", f->model.frame.id, s->sequence);
    csv_cell(f->out, time, 0);
    fprintf(f->out, ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%u,", s->measured_at_ns, meta->id,
            meta->source_id, meta->generation);
    csv_cell(f->out, meta->key, 0);
    fputc(',', f->out);
    csv_cell(f->out, labels, 0);
    fputc(',', f->out);
    csv_cell(f->out, quanta_type_name(meta->type), 0);
    fputc(',', f->out);
    csv_cell(f->out, meta->unit, 0);
    fputc(',', f->out);
    csv_cell(f->out, quanta_status_name(s->status), 0);
    fputc(',', f->out);
    value[0] = 0;
    if (redact)
    {
        quanta_copy(value, sizeof(value), "[redacted]");
    }
    else if (s->status == QUANTA_OK)
    {
        if (s->value.type == QUANTA_STRING)
        {
            quanta_copy(value, sizeof(value), s->value.as.string);
        }
        else
        {
            quanta_format_value(value, sizeof(value), &s->value, meta->unit, NULL, 1);
        }
    }
    csv_cell(f->out, value, f->plan->csv_safe && s->value.type == QUANTA_STRING);
    fprintf(f->out, ",%" PRIu64 ",%" PRIu64 ",%u,", s->window_ns, s->read_duration_ns,
            s->quality_flags);
    csv_cell(f->out, quanta_metric_source(meta->metric), 0);
    fputc(',', f->out);
    csv_cell(f->out, s->message, 1);
    fputc('\n', f->out);
    return ferror(f->out) ? QUANTA_IO_ERROR : QUANTA_OK;
}

static quanta_status begin(void *c, const quanta_plan *p)
{
    file_sink *f = (file_sink *)c;
    (void)p;
    if (f->plan->format == QUANTA_FORMAT_CSV)
    {
        fputs("event,session_id,frame_id,sequence,timestamp_utc,monotonic_ns,"
              "series_id,source_"
              "series_id,generation,key,labels,type,unit,status,value,window_ns,"
              "read_duration_ns,"
              "quality,source,message\n",
              f->out);
    }
    return ferror(f->out) ? QUANTA_IO_ERROR : QUANTA_OK;
}

static quanta_status upsert(void *c, const quanta_series *s)
{
    file_sink *f = (file_sink *)c;
    return f->ms.upsert_series(&f->model, s);
}

static quanta_status remove_series(void *c, uint64_t id)
{
    file_sink *f = (file_sink *)c;
    quanta_item *p = quanta_model_find(&f->model, id);
    quanta_status st = QUANTA_OK;
    if (p && f->plan->format == QUANTA_FORMAT_CSV)
    {
        quanta_sample sample;
        memset(&sample, 0, sizeof(sample));
        sample.series_id = id;
        sample.measured_at_ns = quanta_now_ns();
        sample.status = QUANTA_GONE;
        sample.value.type = p->meta.type;
        st = write_csv(f, &p->meta, &sample, "remove");
    }
    if (st != QUANTA_OK)
    {
        return st;
    }
    return f->ms.remove_series(&f->model, id);
}

static quanta_status frame(void *c, const quanta_frame_info *i)
{
    file_sink *f = (file_sink *)c;
    return f->ms.begin_frame(&f->model, i);
}

static quanta_status sample(void *c, const quanta_sample *s)
{
    file_sink *f = (file_sink *)c;
    quanta_status st = f->ms.emit_sample(&f->model, s);
    quanta_item *p;
    if (st != QUANTA_OK)
    {
        return st;
    }
    if (f->plan->format != QUANTA_FORMAT_CSV)
    {
        return QUANTA_OK;
    }
    p = quanta_model_find(&f->model, s->series_id);
    return p ? write_csv(f, &p->meta, s, "sample") : QUANTA_INVALID_DATA;
}

static void md_cell(FILE *f, const char *s)
{
    while (*s)
    {
        if (*s == '|' || *s == '\\' || *s == '`' || *s == '*' || *s == '_' || *s == '[' ||
            *s == ']')
        {
            fputc('\\', f);
        }
        if (*s == '&')
        {
            fputs("&amp;", f);
        }
        else if (*s == '<' || *s == '>')
        {
            fputs(*s == '<' ? "&lt;" : "&gt;", f);
        }
        else if (*s == '\n' || *s == '\r')
        {
            fputc(' ', f);
        }
        else
        {
            fputc((unsigned char)*s, f);
        }
        s++;
    }
}

static quanta_status end_frame(void *c)
{
    file_sink *f = (file_sink *)c;
    size_t i;
    char utc[64];
    quanta_utc_string(f->model.frame.realtime_ns, utc, sizeof(utc));
    if (f->plan->format == QUANTA_FORMAT_VALUE)
    {
        for (i = 0; i < f->model.count; ++i)
        {
            const quanta_item *item = &f->model.items[i];
            char value[2048];
            if (!item->active)
            {
                continue;
            }
            if (f->plan->redact && (item->meta.metric->flags & QUANTA_SENSITIVE))
            {
                quanta_copy(value, sizeof(value), "[redacted]");
            }
            else if (item->has_last && item->last.status == QUANTA_OK)
            {
                if (item->last.value.type == QUANTA_STRING)
                {
                    quanta_safe_text(value, sizeof(value), item->last.value.as.string);
                }
                else
                {
                    quanta_format_value(value, sizeof(value), &item->last.value, item->meta.unit,
                                        NULL, 1);
                }
            }
            else
            {
                quanta_copy(value, sizeof(value), "--");
            }
            fprintf(f->out, "%s\n", value);
        }
        return fflush(f->out) || ferror(f->out) ? QUANTA_IO_ERROR : QUANTA_OK;
    }
    if (f->plan->format != QUANTA_FORMAT_CSV)
    {
        if (f->plan->format == QUANTA_FORMAT_MD)
        {
            fprintf(f->out,
                    "## Snapshot %" PRIu64 " - %s\n\n| Metric | Value | Status | Age "
                    "(s) |\n|---|---|---|---:|\n",
                    f->model.frame.id, utc);
        }
        else
        {
            fprintf(f->out, "Quanta %s | %s | snapshot %" PRIu64 "\n", QUANTA_VERSION, utc,
                    f->model.frame.id);
            if (f->plan->format == QUANTA_FORMAT_TABLE)
            {
                fprintf(f->out, "%-62s %-30s %-22s %s\n", "METRIC", "VALUE", "STATUS", "AGE(s)");
            }
        }
        for (i = 0; i < f->model.count; i++)
        {
            if (f->model.items[i].active)
            {
                quanta_item *p = &f->model.items[i];
                char name[2048], safe[2048], value[1024], status[128];
                double age = 0;
                quanta_series_name(name, sizeof(name), &p->meta);
                quanta_safe_text(safe, sizeof(safe), name);
                quanta_item_value(value, sizeof(value), p, f->plan->redact);
                quanta_item_status(status, sizeof(status), p, f->model.frame.monotonic_ns);
                if (p->has_last)
                {
                    uint64_t measured = p->last.status != QUANTA_OK && p->has_good
                                            ? p->good.measured_at_ns
                                            : p->last.measured_at_ns;
                    if (f->model.frame.monotonic_ns > measured)
                    {
                        age = (double)(f->model.frame.monotonic_ns - measured) / 1e9;
                    }
                }
                if (f->plan->format == QUANTA_FORMAT_MD)
                {
                    fputs("| ", f->out);
                    md_cell(f->out, safe);
                    fputs(" | ", f->out);
                    md_cell(f->out, value);
                    fprintf(f->out, " | %s | %.3f |\n", status, age);
                }
                else if (f->plan->format == QUANTA_FORMAT_TABLE)
                {
                    fprintf(f->out, "%-62s %-30s %-22s %.3f\n", safe, value, status, age);
                }
                else
                {
                    fprintf(f->out, "  %s\n    %s  [%s; age %.3fs]\n", safe, value, status, age);
                }
            }
        }
        fputc('\n', f->out);
    }
    return fflush(f->out) || ferror(f->out) ? QUANTA_IO_ERROR : QUANTA_OK;
}

static quanta_status diag(void *c, quanta_status s, const char *m)
{
    char safe[512];
    (void)c;
    quanta_safe_text(safe, sizeof(safe), m);
    fprintf(stderr, "quanta: %s: %s\n", quanta_status_name(s), safe);
    return QUANTA_OK;
}

static quanta_status end(void *c, quanta_status s)
{
    file_sink *f = (file_sink *)c;
    (void)s;
    return fflush(f->out) || ferror(f->out) ? QUANTA_IO_ERROR : QUANTA_OK;
}

int quanta_run_file(quanta_engine *e, const quanta_plan *p)
{
    file_sink f;
    quanta_sink s;
    quanta_status result;
    int close_file = 0, rc;
    memset(&f, 0, sizeof(f));
    f.out = stdout;
    f.plan = p;
    quanta_model_init(&f.model, p);
    f.ms = quanta_model_sink(&f.model);
    snprintf(f.session, sizeof(f.session), "%" PRIu64 "-%ld-%" PRIu64, quanta_realtime_ns(),
             (long)quanta_process_id(), quanta_now_ns());
    if (p->output[0] && strcmp(p->output, "-"))
    {
        f.out = quanta_create_output(p->output);
        if (!f.out)
        {
            fprintf(stderr,
                    "quanta: cannot create '%s': %s (output files are never "
                    "overwritten)\n",
                    p->output, strerror(errno));
            quanta_model_destroy(&f.model);
            return 1;
        }
        close_file = 1;
    }
    memset(&s, 0, sizeof(s));
    s.struct_size = sizeof(s);
    s.api_version = 1;
    s.context = &f;
    s.begin_session = begin;
    s.upsert_series = upsert;
    s.remove_series = remove_series;
    s.begin_frame = frame;
    s.emit_sample = sample;
    s.end_frame = end_frame;
    s.emit_diagnostic = diag;
    s.end_session = end;
    result = quanta_engine_run(e, &s);
    rc = result == QUANTA_OK || result == QUANTA_CANCELLED ? 0 : 1;
    if (result != QUANTA_OK && result != QUANTA_CANCELLED)
    {
        fprintf(stderr, "quanta: output/session error: %s\n", quanta_status_name(result));
    }
    if (close_file && fclose(f.out))
    {
        rc = 1;
    }
    quanta_model_destroy(&f.model);
    return rc;
}
