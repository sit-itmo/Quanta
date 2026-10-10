/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "collector_internal.h"
#include "frontend_internal.h"
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks, errors;
#define REQUIRE(x)                                                                                 \
    do                                                                                             \
    {                                                                                              \
        checks++;                                                                                  \
        if (!(x))                                                                                  \
        {                                                                                          \
            quanta_fprintf(quanta_stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            errors++;                                                                              \
        }                                                                                          \
    } while (0)
#if QUANTA_WITH_NETWORK || QUANTA_WITH_HWMON
static void put(const char *root, const char *name, const char *text)
{
    char p[2048];
    quanta_file *f;
    REQUIRE(quanta_path(p, sizeof(p), root, name));
    f = fopen(p, "w");
    REQUIRE(f != NULL);
    if (f)
    {
        REQUIRE(quanta_fputs(text, f) >= 0);
        REQUIRE(quanta_fclose(f) == 0);
    }
}

static void directory(const char *root, const char *name)
{
    char p[2048];
    REQUIRE(quanta_path(p, sizeof(p), root, name));
    REQUIRE(mkdir(p, 0700) == 0);
}

static void erase(const char *root, const char *name, int dir)
{
    char p[2048];
    quanta_path(p, sizeof(p), root, name);
    REQUIRE((dir ? rmdir(p) : unlink(p)) == 0);
}

typedef struct captured
{
    quanta_engine *engine;
    const char *root;
    unsigned added, removed, samples, frames, warm;
    uint64_t first_id, last_id, period;
    quanta_status status;
    double number;
    int replace;
} captured;

static quanta_status upsert(void *ctx, const quanta_series *s)
{
    captured *c = ctx;
    c->added++;
    if (!c->first_id)
    {
        c->first_id = s->id;
    }
    c->last_id = s->id;
    c->period = s->effective_period_ns;
    return QUANTA_OK;
}

static quanta_status remove_series(void *ctx, uint64_t id)
{
    captured *c = ctx;
    c->removed++;
    REQUIRE(id == c->first_id);
    return QUANTA_OK;
}

static quanta_status sample(void *ctx, const quanta_sample *s)
{
    captured *c = ctx;
    c->samples++;
    c->status = s->status;
    if (s->status == QUANTA_WARMUP)
    {
        c->warm++;
    }
    if (s->status == QUANTA_OK)
    {
        REQUIRE(quanta_value_number(&s->value, &c->number));
    }
    return QUANTA_OK;
}

static quanta_status frame(void *ctx)
{
    captured *c = ctx;
    c->frames++;
    if (c->replace && c->frames == 1)
    {
        put(c->root, "class/net/fake0/ifindex", "2\n");
        put(c->root, "net/dev",
            "Inter-| Receive | Transmit\n face | bytes\n fake0: 5 1 0 0 0 0 0 0 7 "
            "1 0 0 0 0 0 0\n");
        quanta_engine_refresh(c->engine);
    }
    return QUANTA_OK;
}

static quanta_sink sink_for(captured *c)
{
    quanta_sink s;
    quanta_memset(&s, 0, sizeof(s));
    s.api_version = 1;
    s.struct_size = sizeof(s);
    s.context = c;
    s.upsert_series = upsert;
    s.remove_series = remove_series;
    s.emit_sample = sample;
    s.end_frame = frame;
    return s;
}
#endif
#if QUANTA_WITH_NETWORK
static void network_test(void)
{
    char dir[] = "/tmp/quanta-hotplug-XXXXXX", error[512];
    quanta_plan *p = quanta_malloc(sizeof(*p));
    quanta_engine *e = NULL;
    quanta_paths paths;
    captured c;
    quanta_sink sink;
    REQUIRE(p != NULL);
    if (!p)
    {
        return;
    }
    REQUIRE(mkdtemp(dir) != NULL);
    directory(dir, "class");
    directory(dir, "class/net");
    directory(dir, "class/net/fake0");
    directory(dir, "net");
    put(dir, "class/net/fake0/ifindex", "1\n");
    put(dir, "net/dev",
        "Inter-| Receive | Transmit\n face | bytes\n fake0: 100 1 0 0 0 0 0 0 "
        "100 1 0 0 0 0 0 0\n");
    paths.proc_root = paths.sys_root = paths.etc_root = dir;
    REQUIRE(quanta_query_parse("mode file; every 100ms; for 600ms; select "
                               "rate(device.net.rx_bytes_total);",
                               p, error, sizeof(error)) == QUANTA_OK);
    REQUIRE(quanta_plan_validate(p, error, sizeof(error)) == QUANTA_OK);
    REQUIRE(quanta_engine_create(p, &paths, &e, error, sizeof(error)) == QUANTA_OK);
    quanta_memset(&c, 0, sizeof(c));
    c.engine = e;
    c.root = dir;
    c.replace = 1;
    sink = sink_for(&c);
    if (e)
    {
        REQUIRE(quanta_engine_run(e, &sink) == QUANTA_OK);
        REQUIRE(c.added == 2 && c.removed == 1);
        REQUIRE(c.first_id != c.last_id);
        REQUIRE(c.warm == 2);
        REQUIRE(c.samples >= 3);
        REQUIRE(c.status == QUANTA_OK && c.number >= 0);
        quanta_engine_destroy(e);
    }
    erase(dir, "class/net/fake0/ifindex", 0);
    erase(dir, "class/net/fake0", 1);
    erase(dir, "class/net", 1);
    erase(dir, "class", 1);
    erase(dir, "net/dev", 0);
    erase(dir, "net", 1);
    REQUIRE(rmdir(dir) == 0);
    quanta_free(p);
}
#endif
#if QUANTA_WITH_HWMON
static void hwmon_test(void)
{
    char dir[] = "/tmp/quanta-hwmon-XXXXXX", error[512];
    quanta_plan *p = quanta_malloc(sizeof(*p));
    quanta_engine *e = NULL;
    quanta_paths paths;
    captured c;
    quanta_sink sink;
    int run;
    REQUIRE(p != NULL);
    if (!p)
    {
        return;
    }
    REQUIRE(mkdtemp(dir) != NULL);
    directory(dir, "class");
    directory(dir, "class/hwmon");
    directory(dir, "class/hwmon/hwmon0");
    put(dir, "class/hwmon/hwmon0/name", "fixture_chip\n");
    put(dir, "class/hwmon/hwmon0/temp1_input", "54321\n");
    put(dir, "class/hwmon/hwmon0/update_interval", "2000\n");
    paths.proc_root = paths.sys_root = paths.etc_root = dir;
    REQUIRE(quanta_query_parse("every 1s; strict; count 1; select sensor.temperature;", p, error,
                               sizeof(error)) == QUANTA_OK);
    REQUIRE(quanta_plan_validate(p, error, sizeof(error)) == QUANTA_OK);
    REQUIRE(quanta_engine_create(p, &paths, &e, error, sizeof(error)) == QUANTA_BAD_QUERY &&
            e == NULL);
    for (run = 0; run < 3; run++)
    {
        REQUIRE(quanta_query_parse("once; select sensor.temperature;", p, error, sizeof(error)) ==
                QUANTA_OK);
        REQUIRE(quanta_plan_validate(p, error, sizeof(error)) == QUANTA_OK);
        if (run == 1)
        {
            put(dir, "class/hwmon/hwmon0/temp1_fault", "1\n");
        }
        if (run == 2)
        {
            put(dir, "class/hwmon/hwmon0/temp1_fault", "0\n");
            put(dir, "class/hwmon/hwmon0/temp1_input", "invalid\n");
        }
        REQUIRE(quanta_engine_create(p, &paths, &e, error, sizeof(error)) == QUANTA_OK);
        quanta_memset(&c, 0, sizeof(c));
        sink = sink_for(&c);
        if (e)
        {
            REQUIRE(quanta_engine_run(e, &sink) == QUANTA_OK);
            REQUIRE(c.added == 1 && c.samples == 1 && c.period == 2 * QUANTA_NS_S);
            REQUIRE(c.status == (run == 0   ? QUANTA_OK
                                 : run == 1 ? QUANTA_NO_DATA
                                            : QUANTA_INVALID_DATA));
            if (!run)
            {
                REQUIRE(c.number > 54.32 && c.number < 54.33);
            }
            quanta_engine_destroy(e);
            e = NULL;
        }
    }
    erase(dir, "class/hwmon/hwmon0/name", 0);
    erase(dir, "class/hwmon/hwmon0/temp1_input", 0);
    erase(dir, "class/hwmon/hwmon0/update_interval", 0);
    erase(dir, "class/hwmon/hwmon0/temp1_fault", 0);
    erase(dir, "class/hwmon/hwmon0", 1);
    erase(dir, "class/hwmon", 1);
    erase(dir, "class", 1);
    REQUIRE(rmdir(dir) == 0);
    quanta_free(p);
}
#endif
int main(void)
{
#if QUANTA_WITH_NETWORK
    network_test();
#endif
#if QUANTA_WITH_HWMON
    hwmon_test();
#endif
    quanta_printf("device fixtures: %u checks, %u failures\n", checks, errors);
    return errors ? 1 : 0;
}
