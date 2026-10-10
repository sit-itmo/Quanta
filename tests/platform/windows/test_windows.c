/* SPDX-License-Identifier: MIT -- native Windows tests, not a mock WinAPI. */
#include "quanta/sensors.h"
#include "quanta/view.h"
#include "quanta/osal.h"
#include "windows_internal.h"
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

static void *thread_worker(void *p)
{
    InterlockedIncrement((volatile LONG *)p);
    return NULL;
}

static void files_and_clock(void)
{
    wchar_t tmp[MAX_PATH], path[MAX_PATH], *wide;
    char utf8[2048], text[128];
    quanta_file *f;
    volatile LONG seen = 0;
    quanta_thread thread;
    quanta_mutex mutex;
    uint64_t t = quanta_now_ns();
    CHECK(t > 0);
    quanta_sleep_ns(20 * QUANTA_NS_MS);
    CHECK(quanta_now_ns() > t);
    CHECK(quanta_realtime_ns() > UINT64_C(1500000000000000000));
    CHECK(quanta_mutex_init(&mutex) == 0);
    quanta_mutex_lock(&mutex);
    quanta_mutex_unlock(&mutex);
    quanta_mutex_destroy(&mutex);
    CHECK(quanta_thread_start(&thread, thread_worker, (void *)&seen) == 0);
    quanta_thread_join(&thread);
    CHECK(seen == 1);
    CHECK(GetTempPathW(MAX_PATH, tmp) > 0);
    swprintf(path, MAX_PATH, L"%squanta-%lu-\x0442\x0435\x0441\x0442.txt", tmp,
             (unsigned long)GetCurrentProcessId());
    CHECK(quanta_win_utf8(utf8, sizeof(utf8), path));
    DeleteFileW(path);
    f = quanta_create_output(utf8);
    CHECK(f != NULL);
    if (f)
    {
        CHECK(quanta_fputs("UTF-8 fixture\n", f) >= 0);
        CHECK(quanta_fclose(f) == 0);
    }
    f = quanta_create_output(utf8);
    CHECK(f == NULL);
    if (f)
    {
        quanta_fclose(f);
    }
    CHECK(quanta_read_text(utf8, text, sizeof(text)) == QUANTA_OK &&
          !quanta_strcmp(text, "UTF-8 fixture\n"));
    wide = quanta_win_widen(utf8);
    CHECK(wide != NULL && wcscmp(wide, path) == 0);
    quanta_free(wide);
    CHECK(DeleteFileW(path));
    CHECK(!quanta_win_widen("\xc0\x80"));
}

static void live_session(void)
{
    quanta_plan *p = (quanta_plan *)quanta_malloc(sizeof(*p));
    quanta_engine *engine = NULL;
    quanta_live *live = NULL;
    quanta_model model;
    quanta_sink sink;
    char error[512];
    quanta_status st;
    uint64_t limit;
    CHECK(p != NULL);
    if (!p)
    {
        return;
    }
    CHECK(quanta_query_parse("mode file; every 250ms; count 3; select os.uptime;", p, error,
                             sizeof(error)) == QUANTA_OK);
    CHECK(quanta_plan_validate(p, error, sizeof(error)) == QUANTA_OK);
    st = quanta_engine_create(p, NULL, &engine, error, sizeof(error));
    CHECK(st == QUANTA_OK);
    if (st != QUANTA_OK)
    {
        quanta_free(p);
        return;
    }
    quanta_model_init(&model, p);
    sink = quanta_model_sink(&model);
    st = quanta_live_start(engine, p, &live);
    CHECK(st == QUANTA_OK);
    if (st == QUANTA_OK)
    {
        limit = quanta_now_ns() + 10 * QUANTA_NS_S;
        while (!quanta_live_done(live) && quanta_now_ns() < limit)
        {
            CHECK(quanta_live_pump(live, &sink) == QUANTA_OK);
            quanta_sleep_ns(10 * QUANTA_NS_MS);
        }
        CHECK(quanta_live_done(live));
        CHECK(quanta_live_pump(live, &sink) == QUANTA_OK);
        st = quanta_live_finish(live, 1);
        CHECK(st == QUANTA_OK);
        CHECK(model.done);
        CHECK(model.end_status == QUANTA_OK);
        CHECK(model.frames == 3);
        CHECK(model.samples >= 1 && model.samples <= model.frames);
    }
    quanta_model_destroy(&model);
    quanta_engine_destroy(engine);
    quanta_free(p);
}

int main(void)
{
    CHECK(quanta_install_signal_handlers());
    files_and_clock();
    live_session();
    quanta_printf("Native Windows: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
