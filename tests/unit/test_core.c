/**
 * @file tests/unit/test_core.c
 *
 * Regression tests for queries, engine, snapshot groups, values and error handling.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "collector_internal.h"
#include "frontend_internal.h"
#include "quanta/view.h"
#include "raster.h"
#include <math.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
static unsigned checks, failures;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        checks++;                                                                                  \
        if (!(x))                                                                                  \
        {                                                                                          \
            quanta_fprintf(quanta_stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static void test_query(void)
{
    quanta_plan *p = (quanta_plan *)quanta_malloc(sizeof(*p));
    char e[512];
    uint64_t ns;
    quanta_status s;
    CHECK(p != NULL);
    if (!p)
    {
        return;
    }
    CHECK(quanta_parse_duration("250ms", &ns) == QUANTA_OK && ns == 250 * QUANTA_NS_MS);
    CHECK(quanta_parse_duration("0.5s", &ns) == QUANTA_OK && ns == 500 * QUANTA_NS_MS);
    CHECK(quanta_parse_duration("5m", &ns) == QUANTA_OK && ns == 300 * QUANTA_NS_S);
    CHECK(quanta_parse_duration("NaNs", &ns) != QUANTA_OK);
    CHECK(quanta_parse_duration("-1s", &ns) != QUANTA_OK);
    CHECK(quanta_parse_duration("1", &ns) != QUANTA_OK);
    CHECK(quanta_parse_duration("999999h", &ns) != QUANTA_OK);
    s = quanta_query_parse("# comment\nversion 1; mode file; format csv; every 250ms; count 3; "
                           "select os.uptime as plot(min=0,title=\"Boot time\") every 1s;",
                           p, e, sizeof(e));
    CHECK(s == QUANTA_OK);
    CHECK(p->select_count == 1 && p->count == 3 && !p->once &&
          p->selects[0].period_ns == QUANTA_NS_S);
    CHECK(quanta_plan_validate(p, e, sizeof(e)) == QUANTA_OK);
    {
        const char *bad[] = {"mode foo; select os.name;",
                             "mode file; mode tui; select os.name;",
                             "select os.name",
                             "select os.name as robot;",
                             "select os.name as value(title=\"unterminated);",
                             "select os.name as value(precision=0.1);",
                             "select os.name{x=\"a\",x=\"b\"};",
                             "select rate(rate(os.uptime));",
                             "select os.name as value(min=nan);",
                             "select os.name as value(title=\"\\q\");",
                             "version 2; select os.name;"};
        size_t i;
        for (i = 0; i < QUANTA_ARRAY_SIZE(bad); i++)
        {
            CHECK(quanta_query_parse(bad[i], p, e, sizeof(e)) != QUANTA_OK);
        }
    }
    CHECK(quanta_query_parse("select os.name as plot;", p, e, sizeof(e)) == QUANTA_OK);
    CHECK(quanta_plan_validate(p, e, sizeof(e)) == QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse("select imaginary.metric;", p, e, sizeof(e)) == QUANTA_OK);
    CHECK(quanta_plan_validate(p, e, sizeof(e)) == QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse("select optional imaginary.metric;", p, e, sizeof(e)) == QUANTA_OK);
    CHECK(quanta_plan_validate(p, e, sizeof(e)) == QUANTA_OK);
    CHECK(quanta_query_parse("once; for 2s; select os.name;", p, e, sizeof(e)) == QUANTA_OK);
    CHECK(quanta_plan_validate(p, e, sizeof(e)) == QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse("select os.name{ifname=\"a\"};", p, e, sizeof(e)) == QUANTA_OK);
    CHECK(quanta_plan_validate(p, e, sizeof(e)) == QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse("select os.uptime as value(unit=\"GiB\");", p, e, sizeof(e)) ==
          QUANTA_OK);
    CHECK(quanta_plan_validate(p, e, sizeof(e)) == QUANTA_BAD_QUERY);
    CHECK(quanta_glob_match("cpu.*", "cpu.core.utilization"));
    CHECK(quanta_glob_match("e?p*", "enp3s0"));
    CHECK(!quanta_glob_match("e?p*", "eth0"));
    CHECK(quanta_glob_match("a**b?c", "axyzbxc"));
    CHECK(!quanta_glob_match("a*b?c", "abc"));
    /* Deterministic parser smoke-fuzzing, including high bytes and unterminated
     * strings. */
    {
        unsigned seed = 0x1a2b3c4d;
        size_t i, j;
        char input[256];
        const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789.;{}()=,\"\\ \n#*/_-";
        for (i = 0; i < 3000; i++)
        {
            size_t len = (seed >> 16) % 255;
            for (j = 0; j < len; j++)
            {
                seed = 1664525u * seed + 1013904223u;
                input[j] = alphabet[seed % (sizeof(alphabet) - 1)];
            }
            input[len] = 0;
            s = quanta_query_parse(input, p, e, sizeof(e));
            CHECK(s == QUANTA_OK || s == QUANTA_BAD_QUERY || s == QUANTA_LIMIT);
            seed = 1664525u * seed + 1013904223u;
        }
    }
    quanta_free(p);
}

static void test_values(void)
{
    quanta_value v;
    quanta_view_spec view;
    char b[256];
    double x;
    quanta_memset(&v, 0, sizeof(v));
    quanta_memset(&view, 0, sizeof(view));
    view.precision = 2;
    v.type = QUANTA_U64;
    v.as.u64 = UINT64_MAX;
    quanta_format_value(b, sizeof(b), &v, "B", NULL, 1);
    CHECK(!quanta_strcmp(b, "18446744073709551615"));
    v.type = QUANTA_F64;
    v.as.f64 = 0.25;
    quanta_format_value(b, sizeof(b), &v, "1", &view, 0);
    CHECK(!quanta_strcmp(b, "25.00 %"));
    CHECK(quanta_unit_scale("B/s", "MiB/s", &x) && quanta_fabs(x - 1.0 / 1048576) < 1e-12);
    CHECK(!quanta_unit_scale("degC", "B", &x));
    CHECK(!quanta_parse_u64("-1", &v.as.u64));
    CHECK(!quanta_parse_u64("18446744073709551616", &v.as.u64));
    CHECK(quanta_parse_u64("42\n", &v.as.u64) && v.as.u64 == 42);
    quanta_safe_text(b, sizeof(b), "A\033[31m\nB");
    CHECK(quanta_strchr(b, '\033') == NULL && quanta_strchr(b, '\n') == NULL);
}

static void test_counters(void)
{
    quanta_sample a, b;
    quanta_value v;
    uint64_t window;
    quanta_memset(&a, 0, sizeof(a));
    quanta_memset(&b, 0, sizeof(b));
    a.status = b.status = QUANTA_OK;
    a.value.type = b.value.type = QUANTA_U64;
    a.counter_epoch = b.counter_epoch = 7;
    a.value.as.u64 = UINT64_MAX - 100;
    b.value.as.u64 = UINT64_MAX - 99;
    a.measured_at_ns = QUANTA_NS_S;
    b.measured_at_ns = a.measured_at_ns + 100 * QUANTA_NS_MS;
    CHECK(quanta_counter_derive(QUANTA_RATE, &a, &b, &v, &window) == QUANTA_OK);
    CHECK(v.type == QUANTA_F64 && quanta_fabs(v.as.f64 - 10) < 1e-9 && window == 100 * QUANTA_NS_MS);
    CHECK(quanta_counter_derive(QUANTA_DELTA, &a, &b, &v, &window) == QUANTA_OK &&
          v.type == QUANTA_U64 && v.as.u64 == 1);
    b.value.as.u64 = 0;
    CHECK(quanta_counter_derive(QUANTA_RATE, &a, &b, &v, &window) == QUANTA_WARMUP);
    b.value.as.u64 = a.value.as.u64 + 1;
    b.counter_epoch = 8;
    CHECK(quanta_counter_derive(QUANTA_RATE, &a, &b, &v, &window) == QUANTA_WARMUP);
    b.counter_epoch = 7;
    b.measured_at_ns = a.measured_at_ns;
    CHECK(quanta_counter_derive(QUANTA_RATE, &a, &b, &v, &window) == QUANTA_WARMUP);
    b.status = QUANTA_PERMISSION_DENIED;
    CHECK(quanta_counter_derive(QUANTA_RATE, &a, &b, &v, &window) == QUANTA_PERMISSION_DENIED);
}

static void test_normalizers(void)
{
#if QUANTA_WITH_MEMORY && !defined(_WIN32)
    quanta_linux_meminfo m;
    CHECK(quanta_native_parse_meminfo("MemTotal: 1000 kB\nMemAvailable: 250 kB\nMemFree: 10 "
                                   "kB\nSwapTotal: 20 kB\nSwapFree: 5 kB\n",
                                   &m) == QUANTA_OK);
    CHECK(m.total == 1024000 && m.available == 256000 && m.found == 31);
    CHECK(quanta_native_parse_meminfo("MemTotal: 100 kB\nMemFree: 10 kB\n", &m) == QUANTA_OK &&
          !(m.found & 2));
    CHECK(quanta_native_parse_meminfo("MemTotal: 100 kB\nMemTotal: 100 kB\n", &m) ==
          QUANTA_INVALID_DATA);
    CHECK(quanta_native_parse_meminfo("MemTotal: 100 kB\nMemAvailable: 101 kB\n", &m) ==
          QUANTA_INVALID_DATA);
    CHECK(quanta_native_parse_meminfo("MemTotal: 18446744073709551615 kB\n", &m) ==
          QUANTA_INVALID_DATA);
    CHECK(quanta_native_parse_meminfo("MemTotal: 100 MB\n", &m) == QUANTA_INVALID_DATA);
    CHECK(quanta_native_parse_meminfo("Unknown: 0\n", &m) == QUANTA_NO_DATA);
#endif
#if QUANTA_WITH_CPU && !defined(_WIN32)
    {
        uint64_t p[8] = {100, 0, 50, 800, 50, 0, 0, 0}, n[8] = {120, 0, 60, 850, 70, 0, 0, 0};
        double v;
        CHECK(quanta_cpu_fraction(p, n, &v) == QUANTA_OK && quanta_fabs(v - .3) < 1e-9);
        n[4] = 49;
        CHECK(quanta_cpu_fraction(p, n, &v) == QUANTA_WARMUP);
        CHECK(quanta_cpu_fraction(p, p, &v) == QUANTA_WARMUP);
    }
#endif
}

static void test_model(void)
{
    quanta_plan *plan = (quanta_plan *)quanta_malloc(sizeof(*plan));
    quanta_model m, copy;
    quanta_sink sink;
    quanta_series series;
    quanta_sample sample;
    quanta_item *item;
    CHECK(plan != NULL);
    if (!plan)
    {
        return;
    }
    quanta_plan_init(plan);
    plan->history_ns = 2 * QUANTA_NS_S;
    quanta_model_init(&m, plan);
    quanta_memset(&copy, 0, sizeof(copy));
    sink = quanta_model_sink(&m);
    quanta_memset(&series, 0, sizeof(series));
    series.id = 1;
    series.source_id = 2;
    series.metric = quanta_metric_find("os.uptime");
    series.type = QUANTA_F64;
    series.effective_period_ns = QUANTA_NS_S;
    series.view.widget = QUANTA_WIDGET_PLOT;
    quanta_copy(series.key, sizeof(series.key), "os.uptime");
    quanta_copy(series.unit, sizeof(series.unit), "s");
    CHECK(sink.upsert_series(sink.context, &series) == QUANTA_OK);
    quanta_memset(&sample, 0, sizeof(sample));
    sample.series_id = 1;
    sample.sequence = 1;
    sample.status = QUANTA_OK;
    sample.value.type = QUANTA_F64;
    sample.value.as.f64 = 5;
    sample.measured_at_ns = QUANTA_NS_S;
    CHECK(sink.emit_sample(sink.context, &sample) == QUANTA_OK);
    CHECK(sink.emit_sample(sink.context, &sample) == QUANTA_OK);
    item = quanta_model_find(&m, 1);
    CHECK(item && item->history_count == 1 && m.samples == 1);
    CHECK(quanta_model_clone(&copy, &m) == QUANTA_OK);
    CHECK(copy.items[0].history != m.items[0].history);
    sample.sequence = 2;
    sample.status = QUANTA_IO_ERROR;
    sample.measured_at_ns += QUANTA_NS_S;
    CHECK(sink.emit_sample(sink.context, &sample) == QUANTA_OK);
    CHECK(item->good.value.as.f64 == 5 && item->last.status == QUANTA_IO_ERROR &&
          item->history_count == 2);
    CHECK(copy.items[0].last.status == QUANTA_OK);
    sample.sequence = 3;
    sample.status = QUANTA_OK;
    sample.value.as.f64 = 9;
    sample.measured_at_ns += 3 * QUANTA_NS_S;
    CHECK(sink.emit_sample(sink.context, &sample) == QUANTA_OK);
    CHECK(item->history_count == 1 && quanta_history_at(item, 0)->value == 9);
    CHECK(sink.remove_series(sink.context, 1) == QUANTA_OK && m.history_bytes == 0 &&
          !quanta_model_find(&m, 1));
    quanta_model_destroy(&copy);
    quanta_model_destroy(&m);
    quanta_free(plan);
}

static void test_raster(void)
{
    uint32_t pixels[64];
    quanta_surface s = {pixels, 8, 8, 8};
    quanta_clip clip = {0, 0, 8, 8};
    quanta_vertex a[3] = {
        {0, 0, 0, 0, 0x800000ff}, {8, 0, 0, 0, 0x800000ff}, {8, 8, 0, 0, 0x800000ff}};
    quanta_vertex b[3] = {
        {0, 0, 0, 0, 0x800000ff}, {8, 8, 0, 0, 0x800000ff}, {0, 8, 0, 0, 0x800000ff}};
    size_t i;
    quanta_raster_clear(&s, 0);
    quanta_raster_triangle(&s, &clip, a, NULL);
    quanta_raster_triangle(&s, &clip, b, NULL);
    for (i = 0; i < 64; i++)
    {
        CHECK(pixels[i] == 0x800000);
    }
    /* Shared diagonal blended exactly once. */ quanta_raster_clear(&s, 0);
    clip.right = 4;
    quanta_raster_triangle(&s, &clip, a, NULL);
    quanta_raster_triangle(&s, &clip, b, NULL);
    for (i = 0; i < 64; i++)
    {
        CHECK(pixels[i] == (i % 8 < 4 ? 0x800000u : 0));
    }
    a[0].x = INFINITY;
    quanta_raster_triangle(&s, &clip, a, NULL);
    CHECK(pixels[0] == 0x800000);
    {
        uint8_t alpha = 0;
        quanta_texture t = {&alpha, 1, 1, 1, 0};
        quanta_raster_clear(&s, 0);
        quanta_raster_triangle(&s, &clip, b, &t);
        for (i = 0; i < 64; i++)
        {
            CHECK(pixels[i] == 0);
        }
    }
}
#if QUANTA_WITH_MEMORY && !defined(_WIN32)
static void write_file(const char *p, const char *s)
{
    quanta_file *f = fopen(p, "w");
    CHECK(f != NULL);
    if (f)
    {
        CHECK(quanta_fputs(s, f) >= 0);
        CHECK(!quanta_fclose(f));
    }
}

typedef struct capture
{
    unsigned series, samples, ended;
    uint64_t source[4];
    quanta_status result;
    quanta_value values[4];
} capture;

static quanta_status capture_series(void *ctx, const quanta_series *s)
{
    capture *c = (capture *)ctx;
    if (c->series < 4)
    {
        c->source[c->series] = s->source_id;
    }
    c->series++;
    return QUANTA_OK;
}

static quanta_status capture_sample(void *ctx, const quanta_sample *s)
{
    capture *c = (capture *)ctx;
    if (c->samples < 4)
    {
        c->values[c->samples] = s->value;
    }
    c->samples++;
    CHECK(s->status == QUANTA_OK);
    return QUANTA_OK;
}

static quanta_status capture_end(void *ctx, quanta_status s)
{
    capture *c = (capture *)ctx;
    c->result = s;
    c->ended++;
    return QUANTA_OK;
}

static void test_engine_fixture(void)
{
    char dir[] = "/tmp/quanta-unit-XXXXXX", path[1024], error[512];
    quanta_paths paths;
    quanta_plan *p = (quanta_plan *)quanta_malloc(sizeof(*p));
    quanta_engine *e = NULL;
    quanta_sink sink;
    capture cap;
    CHECK(p != NULL);
    if (!p)
    {
        return;
    }
    CHECK(mkdtemp(dir) != NULL);
    quanta_snprintf(path, sizeof(path), "%s/meminfo", dir);
    write_file(path, "MemTotal: 1000 kB\nMemAvailable: 250 kB\n");
    paths.proc_root = dir;
    paths.sys_root = dir;
    paths.etc_root = dir;
    CHECK(quanta_query_parse("once; select os.memory.used_bytes; select os.memory.used_bytes as "
                             "plot; select os.memory.utilization;",
                             p, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_plan_validate(p, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_engine_create(p, &paths, &e, error, sizeof(error)) == QUANTA_OK);
    quanta_memset(&cap, 0, sizeof(cap));
    quanta_memset(&sink, 0, sizeof(sink));
    sink.struct_size = sizeof(sink);
    sink.api_version = 1;
    sink.context = &cap;
    sink.upsert_series = capture_series;
    sink.emit_sample = capture_sample;
    sink.end_session = capture_end;
    if (e)
    {
        CHECK(quanta_engine_run(e, &sink) == QUANTA_OK);
        CHECK(cap.series == 3 && cap.samples == 3 && cap.ended == 1);
        CHECK(cap.source[0] == cap.source[1] && cap.source[0] != cap.source[2]);
        CHECK(cap.values[0].type == QUANTA_U64 && cap.values[0].as.u64 == 750 * 1024);
        CHECK(cap.values[2].type == QUANTA_F64 && quanta_fabs(cap.values[2].as.f64 - .75) < 1e-9);
        CHECK(quanta_engine_metric_acquisitions(e, "os.memory.used_bytes") == 1);
        quanta_engine_destroy(e);
    }
    unlink(path);
    rmdir(dir);
    quanta_free(p);
}
#endif
int main(void)
{
    char err[512];
    CHECK(quanta_registry_validate(err, sizeof(err)) == QUANTA_OK);
    test_query();
    test_values();
    test_counters();
    test_normalizers();
    test_model();
    test_raster();
#if QUANTA_WITH_MEMORY && !defined(_WIN32)
    test_engine_fixture();
#endif
    quanta_printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
