/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "core_internal.h"

typedef struct parser
{
    const char *src;
    size_t pos;
    unsigned line, col, tline, tcol;
    int kind, failed;
    char text[QUANTA_TEXT_CAP];
    char *error;
    size_t error_cap;
} parser;

enum
{
    TOK_END = 0,
    TOK_WORD = 256,
    TOK_STRING
};
#if defined(__GNUC__) || defined(__clang__)
static int fail(parser *, const char *, ...) __attribute__((format(quanta_printf, 2, 3)));
#endif
static int fail(parser *p, const char *fmt, ...)
{
    va_list a;
    char msg[256];
    if (p->failed)
    {
        return 0;
    }
    p->failed = 1;
    va_start(a, fmt);
    quanta_vsnprintf(msg, sizeof(msg), fmt, a);
    va_end(a);
    quanta_snprintf(p->error, p->error_cap, "line %u, column %u: %s", p->tline, p->tcol, msg);
    return 0;
}

static char take(parser *p)
{
    char c = p->src[p->pos++];
    if (c == '\n')
    {
        p->line++;
        p->col = 1;
    }
    else
    {
        p->col++;
    }
    return c;
}

static int wordchar(unsigned char c)
{
    return quanta_isalnum(c) || c == '_' || c == '.' || c == '*' || c == '?' || c == '-' || c == '/' ||
           c == '%';
}

static int next(parser *p)
{
    size_t n = 0;
    char c;
    p->text[0] = 0;
    for (;;)
    {
        while (quanta_isspace((unsigned char)p->src[p->pos]))
        {
            take(p);
        }
        if (p->src[p->pos] == '#' || (p->src[p->pos] == '/' && p->src[p->pos + 1] == '/'))
        {
            while (p->src[p->pos] && p->src[p->pos] != '\n')
            {
                take(p);
            }
            continue;
        }
        break;
    }
    p->tline = p->line;
    p->tcol = p->col;
    c = p->src[p->pos];
    if (!c)
    {
        p->kind = TOK_END;
        return 1;
    }
    if (c == '"')
    {
        take(p);
        while ((c = p->src[p->pos]) && c != '"')
        {
            c = take(p);
            if (c == '\\')
            {
                c = p->src[p->pos];
                if (!c)
                {
                    return fail(p, "unterminated escape");
                }
                take(p);
                if (c == 'n')
                {
                    c = '\n';
                }
                else if (c == 'r')
                {
                    c = '\r';
                }
                else if (c == 't')
                {
                    c = '\t';
                }
                else if (c != '\\' && c != '"')
                {
                    return fail(p, "unsupported string escape");
                }
            }
            if (n + 1 >= sizeof(p->text))
            {
                return fail(p, "token is too long");
            }
            p->text[n++] = c;
        }
        if (!c)
        {
            return fail(p, "unterminated string");
        }
        take(p);
        p->kind = TOK_STRING;
    }
    else if (wordchar((unsigned char)c))
    {
        while (wordchar((unsigned char)p->src[p->pos]))
        {
            if (n + 1 >= sizeof(p->text))
            {
                return fail(p, "token is too long");
            }
            p->text[n++] = take(p);
        }
        p->kind = TOK_WORD;
    }
    else
    {
        p->kind = (unsigned char)take(p);
        p->text[n++] = c;
    }
    p->text[n] = 0;
    return 1;
}

static int isword(parser *p, const char *s)
{
    return p->kind == TOK_WORD && !quanta_strcmp(p->text, s);
}

static int expect(parser *p, int c)
{
    if (p->kind != c)
    {
        return fail(p, "expected '%c', got '%s'", c, p->text);
    }
    return next(p);
}

static int copytok(parser *p, char *dst, size_t cap)
{
    if (p->kind != TOK_WORD && p->kind != TOK_STRING)
    {
        return fail(p, "expected a name or string");
    }
    if (!quanta_copy(dst, cap, p->text))
    {
        return fail(p, "value is too long");
    }
    return next(p);
}

void quanta_plan_init(quanta_plan *q)
{
    quanta_memset(q, 0, sizeof(*q));
    q->mode = QUANTA_MODE_FILE;
    q->format = QUANTA_FORMAT_HUMAN;
    q->once = 1;
    q->period_ns = QUANTA_NS_S;
    q->redraw_ns = 200 * QUANTA_NS_MS;
    q->history_ns = 300 * QUANTA_NS_S;
    q->columns = 2;
    q->csv_safe = 1;
}

quanta_status quanta_parse_duration(const char *s, uint64_t *ns)
{
    char *end;
    double d, m;
    if (!s || !ns)
    {
        return QUANTA_INVALID_DATA;
    }
    quanta_errno = 0;
    d = quanta_strtod(s, &end);
    if (quanta_errno || end == s || !quanta_isfinite(d) || d <= 0)
    {
        return QUANTA_BAD_QUERY;
    }
    if (!quanta_strcmp(end, "ns"))
    {
        m = 1;
    }
    else if (!quanta_strcmp(end, "us"))
    {
        m = 1000;
    }
    else if (!quanta_strcmp(end, "ms"))
    {
        m = 1000000;
    }
    else if (!quanta_strcmp(end, "s"))
    {
        m = 1e9;
    }
    else if (!quanta_strcmp(end, "m"))
    {
        m = 60e9;
    }
    else if (!quanta_strcmp(end, "h"))
    {
        m = 3600e9;
    }
    else
    {
        return QUANTA_BAD_QUERY;
    }
    if (d * m < 1 || d * m > 604800e9)
    {
        return QUANTA_BAD_QUERY;
    }
    *ns = (uint64_t)(d * m + 0.5);
    return QUANTA_OK;
}

static int duration(parser *p, uint64_t *v)
{
    if (p->kind != TOK_WORD || quanta_parse_duration(p->text, v) != QUANTA_OK)
    {
        return fail(p, "invalid duration; use e.g. 250ms, 1s, 5m (max 7d)");
    }
    return next(p);
}

static int number(parser *p, double *v)
{
    char *end;
    quanta_errno = 0;
    *v = quanta_strtod(p->text, &end);
    if (p->kind != TOK_WORD || quanta_errno || *end || end == p->text || !quanta_isfinite(*v))
    {
        return fail(p, "expected a finite number");
    }
    return next(p);
}

static int widget(parser *p, quanta_view_spec *v)
{
    const char *names[] = {"value", "text", "bar", "gauge", "plot", "table"};
    size_t i;
    unsigned seen = 0;
    for (i = 0; i < QUANTA_ARRAY_SIZE(names); i++)
    {
        if (isword(p, names[i]))
        {
            break;
        }
    }
    if (i == QUANTA_ARRAY_SIZE(names))
    {
        return fail(p, "unknown widget '%s'", p->text);
    }
    v->widget = (quanta_widget)i;
    if (!next(p))
    {
        return 0;
    }
    if (p->kind != '(')
    {
        return 1;
    }
    if (!next(p))
    {
        return 0;
    }
    while (p->kind != ')')
    {
        char k[32];
        unsigned bit = 0;
        if (!copytok(p, k, sizeof(k)) || !expect(p, '='))
        {
            return 0;
        }
        if (!quanta_strcmp(k, "title"))
        {
            bit = 1;
            if (!copytok(p, v->title, sizeof(v->title)))
            {
                return 0;
            }
        }
        else if (!quanta_strcmp(k, "unit"))
        {
            bit = 2;
            if (!copytok(p, v->unit, sizeof(v->unit)))
            {
                return 0;
            }
        }
        else if (!quanta_strcmp(k, "min"))
        {
            bit = 4;
            v->has_min = 1;
            if (!number(p, &v->min))
            {
                return 0;
            }
        }
        else if (!quanta_strcmp(k, "max"))
        {
            bit = 8;
            v->has_max = 1;
            if (!number(p, &v->max))
            {
                return 0;
            }
        }
        else if (!quanta_strcmp(k, "precision"))
        {
            double n;
            bit = 16;
            if (!number(p, &n))
            {
                return 0;
            }
            if (n < 0 || n > 9 || n != quanta_floor(n))
            {
                return fail(p, "precision must be an integer in 0..9");
            }
            v->precision = (int)n;
        }
        else
        {
            return fail(p, "unknown widget option '%s'", k);
        }
        if (seen & bit)
        {
            return fail(p, "duplicate widget option '%s'", k);
        }
        seen |= bit;
        if (p->kind == ')')
        {
            break;
        }
        if (!expect(p, ','))
        {
            return 0;
        }
        if (p->kind == ')')
        {
            return fail(p, "trailing comma is not allowed");
        }
    }
    return expect(p, ')');
}

static int selection(parser *p, quanta_plan *q)
{
    quanta_select *s;
    int wrapped = 0;
    if (q->select_count == QUANTA_MAX_SELECT)
    {
        return fail(p, "too many selections (max %d)", QUANTA_MAX_SELECT);
    }
    s = &q->selects[q->select_count++];
    quanta_memset(s, 0, sizeof(*s));
    s->view.precision = 2;
    if (isword(p, "optional"))
    {
        s->optional = 1;
        if (!next(p))
        {
            return 0;
        }
    }
    if (isword(p, "rate") || isword(p, "delta"))
    {
        wrapped = 1;
        s->transform = isword(p, "rate") ? QUANTA_RATE : QUANTA_DELTA;
        if (!next(p) || !expect(p, '('))
        {
            return 0;
        }
    }
    if (isword(p, "provider"))
    {
        return fail(p, "provider() was removed; select a distinct metric name and use hal \"name\"; for hardware access");
    }
    if (p->kind != TOK_WORD)
    {
        return fail(p, "expected metric selector");
    }
    if (!copytok(p, s->pattern, sizeof(s->pattern)))
    {
        return 0;
    }
    if (p->kind == '{')
    {
        if (!next(p))
        {
            return 0;
        }
        while (p->kind != '}')
        {
            char key[QUANTA_NAME_CAP], val[QUANTA_TEXT_CAP];
            if (!copytok(p, key, sizeof(key)) || !expect(p, '=') || !copytok(p, val, sizeof(val)))
            {
                return 0;
            }
            if (quanta_label_get(s->labels, s->label_count, key))
            {
                return fail(p, "duplicate label '%s'", key);
            }
            if (!quanta_label_set(s->labels, &s->label_count, key, val))
            {
                return fail(p, "too many labels");
            }
            if (p->kind == '}')
            {
                break;
            }
            if (!expect(p, ','))
            {
                return 0;
            }
            if (p->kind == '}')
            {
                return fail(p, "trailing comma is not allowed");
            }
        }
        if (!expect(p, '}'))
        {
            return 0;
        }
    }
    if (wrapped && !expect(p, ')'))
    {
        return 0;
    }
    if (isword(p, "as"))
    {
        if (!next(p) || !widget(p, &s->view))
        {
            return 0;
        }
    }
    if (isword(p, "every"))
    {
        if (!next(p) || !duration(p, &s->period_ns))
        {
            return 0;
        }
    }
    return expect(p, ';');
}

quanta_status quanta_query_parse(const char *src, quanta_plan *q, char *err, size_t cap)
{
    parser p;
    uint32_t seen = 0;
    int has_every = 0;
    if (!src || !q || (cap && !err))
    {
        return QUANTA_INVALID_DATA;
    }
    quanta_memset(&p, 0, sizeof(p));
    p.src = src;
    p.line = 1;
    p.col = 1;
    p.error = err;
    p.error_cap = cap;
    if (cap)
    {
        err[0] = 0;
    }
    quanta_plan_init(q);
    if (quanta_strlen(src) > QUANTA_QUERY_CAP)
    {
        quanta_snprintf(err, cap, "query exceeds %d bytes", QUANTA_QUERY_CAP);
        return QUANTA_LIMIT;
    }
    if (!next(&p))
    {
        return QUANTA_BAD_QUERY;
    }
    while (p.kind != TOK_END && !p.failed)
    {
        char directive[32];
        uint32_t bit = 0;
        if (p.kind != TOK_WORD)
        {
            fail(&p, "expected directive, got '%s'", p.text);
            break;
        }
        if (!copytok(&p, directive, sizeof(directive)))
        {
            break;
        }
        if (!quanta_strcmp(directive, "select"))
        {
            if (!selection(&p, q))
            {
                break;
            }
            continue;
        }
        if (!quanta_strcmp(directive, "version"))
        {
            bit = 1;
            if (!isword(&p, "1"))
            {
                fail(&p, "only Quanta Query version 1 is supported");
                break;
            }
            next(&p);
        }
        else if (!quanta_strcmp(directive, "mode"))
        {
            bit = 2;
            if (isword(&p, "file") || isword(&p, "cli"))
            {
                q->mode = QUANTA_MODE_FILE;
            }
            else if (isword(&p, "tui"))
            {
                q->mode = QUANTA_MODE_TUI;
            }
            else if (isword(&p, "gui"))
            {
                q->mode = QUANTA_MODE_GUI;
            }
            else
            {
                fail(&p, "mode must be file, tui or gui");
                break;
            }
            next(&p);
        }
        else if (!quanta_strcmp(directive, "format"))
        {
            bit = 4;
            if (isword(&p, "human"))
            {
                q->format = QUANTA_FORMAT_HUMAN;
            }
            else if (isword(&p, "table"))
            {
                q->format = QUANTA_FORMAT_TABLE;
            }
            else if (isword(&p, "csv"))
            {
                q->format = QUANTA_FORMAT_CSV;
            }
            else if (isword(&p, "md"))
            {
                q->format = QUANTA_FORMAT_MD;
            }
            else if (isword(&p, "value"))
            {
                q->format = QUANTA_FORMAT_VALUE;
            }
            else
            {
                fail(&p, "format must be human, table, csv, md or value");
                break;
            }
            next(&p);
        }
        else if (!quanta_strcmp(directive, "once"))
        {
            bit = 8;
            q->once_explicit = 1;
        }
        else if (!quanta_strcmp(directive, "every"))
        {
            bit = 16;
            has_every = 1;
            if (!duration(&p, &q->period_ns))
            {
                break;
            }
        }
        else if (!quanta_strcmp(directive, "redraw"))
        {
            bit = 32;
            if (!duration(&p, &q->redraw_ns))
            {
                break;
            }
        }
        else if (!quanta_strcmp(directive, "history"))
        {
            bit = 64;
            if (!duration(&p, &q->history_ns))
            {
                break;
            }
        }
        else if (!quanta_strcmp(directive, "for"))
        {
            bit = 128;
            if (!duration(&p, &q->duration_ns))
            {
                break;
            }
        }
        else if (!quanta_strcmp(directive, "output"))
        {
            bit = 256;
            if (!copytok(&p, q->output, sizeof(q->output)))
            {
                break;
            }
        }
        else if (!quanta_strcmp(directive, "strict"))
        {
            bit = 512;
            q->strict_poll = 1;
        }
        else if (!quanta_strcmp(directive, "count"))
        {
            bit = 1024;
            if (p.kind != TOK_WORD || !quanta_parse_u64(p.text, &q->count) || !q->count)
            {
                fail(&p, "count must be a positive integer");
                break;
            }
            next(&p);
        }
        else if (!quanta_strcmp(directive, "hal"))
        {
            bit = 4096;
            if (!copytok(&p, q->hal, sizeof(q->hal))) break;
            if (!q->hal[0]) { fail(&p, "HAL name must not be empty"); break; }
        }
        else if (!quanta_strcmp(directive, "layout"))
        {
            uint64_t n;
            bit = 2048;
            if (!isword(&p, "grid") || !next(&p) || !expect(&p, '(') || !isword(&p, "columns") ||
                !next(&p) || !expect(&p, '='))
            {
                fail(&p, "expected layout grid(columns=N)");
                break;
            }
            if (!quanta_parse_u64(p.text, &n) || n < 1 || n > 8)
            {
                fail(&p, "columns must be in 1..8");
                break;
            }
            q->columns = (unsigned)n;
            if (!next(&p) || !expect(&p, ')'))
            {
                break;
            }
        }
        else
        {
            fail(&p, "unknown directive '%s'", directive);
            break;
        }
        if (seen & bit)
        {
            fail(&p, "duplicate directive '%s'", directive);
            break;
        }
        seen |= bit;
        if (!expect(&p, ';'))
        {
            break;
        }
    }
    if (p.failed)
    {
        return QUANTA_BAD_QUERY;
    }
    q->once = q->once_explicit ||
              !(has_every || q->duration_ns || q->count || q->mode != QUANTA_MODE_FILE);
    return QUANTA_OK;
}

static int schema_has(const char *schema, const char *label)
{
    size_t n = quanta_strlen(label);
    const char *p = schema;
    while (*p)
    {
        const char *e = quanta_strchr(p, ',');
        size_t len = e ? (size_t)(e - p) : quanta_strlen(p);
        if (len == n && !quanta_memcmp(p, label, n))
        {
            return 1;
        }
        if (!e)
        {
            break;
        }
        p = e + 1;
    }
    return 0;
}

quanta_status quanta_plan_validate_with_catalog(quanta_plan *p, const quanta_catalog *catalog,
                                                char *err, size_t cap)
{
    size_t i, j, k, n;
    const quanta_metric_desc *const *r;
#define BAD(...)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        quanta_snprintf(err, cap, __VA_ARGS__);                                                           \
        return QUANTA_BAD_QUERY;                                                                   \
    } while (0)
    if (!p || !catalog || (cap && !err))
    {
        return QUANTA_INVALID_DATA;
    }
    if (quanta_catalog_validate(catalog, err, cap) != QUANTA_OK)
    {
        return QUANTA_INVALID_DATA;
    }
    r = catalog->metrics;
    n = catalog->count;
    if (p->select_count > QUANTA_MAX_SELECT)
    {
        BAD("selection count exceeds capacity");
    }
    if ((unsigned)p->mode > QUANTA_MODE_GUI || (unsigned)p->format > QUANTA_FORMAT_VALUE)
    {
        BAD("invalid mode or format");
    }
    if (!quanta_memchr(p->output, 0, sizeof(p->output)) || !quanta_memchr(p->font_path, 0, sizeof(p->font_path)))
    {
        BAD("unterminated plan path");
    }
    if (!quanta_memchr(p->hal, 0, sizeof(p->hal))) BAD("unterminated HAL name");
    for (i = 0; p->hal[i]; ++i)
    {
        unsigned char ch = (unsigned char)p->hal[i];
        if (!(quanta_isalnum(ch) || ch == '-' || ch == '_' || ch == '.')) BAD("invalid HAL name");
    }
    if (!p->columns || p->columns > 8)
    {
        BAD("columns must be in 1..8");
    }
    if (p->period_ns > 7 * 86400 * QUANTA_NS_S || p->redraw_ns > 7 * 86400 * QUANTA_NS_S ||
        p->duration_ns > 7 * 86400 * QUANTA_NS_S || p->warmup_ns > 7 * 86400 * QUANTA_NS_S)
    {
        BAD("duration exceeds 7 days");
    }
    if (!p->select_count)
    {
        BAD("query has no select statements");
    }
    if (p->period_ns < 10 * QUANTA_NS_MS || p->redraw_ns < 10 * QUANTA_NS_MS)
    {
        BAD("period/redraw must be at least 10ms");
    }
    if (p->history_ns > 86400 * QUANTA_NS_S)
    {
        BAD("history is limited to 24h");
    }
    if (p->count > 1000000)
    {
        BAD("count is limited to 1000000");
    }
    if (p->once && (p->count || p->duration_ns))
    {
        BAD("once cannot be combined with count/for");
    }
    if (p->mode != QUANTA_MODE_FILE && p->once_explicit)
    {
        BAD("once is only valid in file mode");
    }
    if (p->mode != QUANTA_MODE_FILE)
    {
        p->once = 0;
    }
    if (p->mode != QUANTA_MODE_FILE && p->output[0])
    {
        BAD("output is only valid in file mode");
    }
    if (p->mode != QUANTA_MODE_GUI && p->font_path[0])
    {
        BAD("--font is only valid in GUI mode");
    }
    for (i = 0; i < p->select_count; i++)
    {
        quanta_select *s = &p->selects[i];
        size_t matched = 0, compatible = 0;
        if (!quanta_memchr(s->pattern, 0, sizeof(s->pattern)) || !s->pattern[0] ||
            s->label_count > QUANTA_LABEL_CAP ||
            (unsigned)s->transform > QUANTA_DELTA ||
            (unsigned)s->view.widget > QUANTA_WIDGET_TABLE ||
            !quanta_memchr(s->view.title, 0, sizeof(s->view.title)) ||
            !quanta_memchr(s->view.unit, 0, sizeof(s->view.unit)) || !quanta_isfinite(s->view.min) ||
            !quanta_isfinite(s->view.max) || s->view.precision < 0 || s->view.precision > 9 ||
            s->period_ns > 7 * 86400 * QUANTA_NS_S)
        {
            BAD("selection %zu: invalid structure", i + 1);
        }
        for (k = 0; k < s->label_count; ++k)
        {
            size_t previous;
            if (!quanta_memchr(s->labels[k].key, 0, sizeof(s->labels[k].key)) || !s->labels[k].key[0] ||
                !quanta_memchr(s->labels[k].value, 0, sizeof(s->labels[k].value)))
            {
                BAD("invalid label");
            }
            for (previous = 0; previous < k; ++previous)
            {
                if (!quanta_strcmp(s->labels[previous].key, s->labels[k].key))
                {
                    BAD("duplicate label");
                }
            }
        }
        if (!s->period_ns)
        {
            s->period_ns = p->period_ns;
        }
        if (s->period_ns < 10 * QUANTA_NS_MS)
        {
            BAD("selection %zu: period must be at least 10ms", i + 1);
        }
        if (s->view.has_min && s->view.has_max && s->view.max <= s->view.min)
        {
            BAD("selection %zu: max must exceed min", i + 1);
        }
        for (j = 0; j < n; j++)
        {
            if (quanta_catalog_selects(catalog, s, r[j]))
            {
                int labels_ok = 1;
                double scale;
                char unit[32];
                quanta_value_type type = r[j]->type;
                matched++;
                for (k = 0; k < s->label_count; k++)
                {
                    if (!schema_has(r[j]->label_schema, s->labels[k].key))
                    {
                        labels_ok = 0;
                    }
                }
                if (!labels_ok)
                {
                    continue;
                }
                compatible++;
                if (s->transform != QUANTA_RAW &&
                    (r[j]->kind != QUANTA_COUNTER || r[j]->type != QUANTA_U64))
                {
                    BAD("%s: rate/delta requires a u64 counter", r[j]->key);
                }
                if (s->transform == QUANTA_RATE)
                {
                    type = QUANTA_F64;
                }
                if ((s->view.widget == QUANTA_WIDGET_BAR || s->view.widget == QUANTA_WIDGET_GAUGE ||
                     s->view.widget == QUANTA_WIDGET_PLOT) &&
                    (type == QUANTA_STRING || type == QUANTA_BYTES || type == QUANTA_BOOL))
                {
                    BAD("%s: selected widget requires a numeric metric", r[j]->key);
                }
                quanta_snprintf(unit, sizeof(unit), "%s%s", r[j]->unit,
                         s->transform == QUANTA_RATE ? "/s" : "");
                if (s->view.unit[0] && !quanta_unit_scale(unit, s->view.unit, &scale))
                {
                    BAD("%s: cannot convert %s to %s", r[j]->key, unit, s->view.unit);
                }
                if (p->strict_poll && r[j]->refresh != QUANTA_REFRESH_ONCE &&
                    s->period_ns < r[j]->min_poll_ns)
                {
                    BAD("%s: requested period is below the minimum of %.3fs", r[j]->key,
                        (double)r[j]->min_poll_ns / 1e9);
                }
            }
        }
        if (!matched && !s->optional)
        {
            BAD("no compiled metric matches '%s' (use 'quanta list')", s->pattern);
        }
        if (matched && !compatible)
        {
            BAD("'%s': label filter is not supported by matching metric schemas", s->pattern);
        }
    }
    return QUANTA_OK;
#undef BAD
}
