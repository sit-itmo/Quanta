/* SPDX-License-Identifier: MIT -- exercise the real native OSAL behind its
 * abstraction. */
#include "quanta/osal.h"
#include "quanta/quanta.h"
#include "thread.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks, failures;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        ++checks;                                                                                  \
        if (!(x))                                                                                  \
        {                                                                                          \
            ++failures;                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                \
        }                                                                                          \
    } while (0)

typedef struct shared_state
{
    quanta_mutex mutex;
    quanta_condition ready;
    unsigned count, arrived;
    int start;
} shared_state;

static void *worker(void *context)
{
    shared_state *shared = (shared_state *)context;
    quanta_mutex_lock(&shared->mutex);
    shared->arrived++;
    quanta_condition_broadcast(&shared->ready);
    while (!shared->start)
    {
        quanta_condition_wait(&shared->ready, &shared->mutex);
    }
    quanta_mutex_unlock(&shared->mutex);
    for (unsigned i = 0; i < 5000; i++)
    {
        quanta_mutex_lock(&shared->mutex);
        shared->count++;
        quanta_mutex_unlock(&shared->mutex);
    }
    return NULL;
}

static void threads(void)
{
    shared_state shared = {0};
    quanta_thread workers[2];
    unsigned started = 0;
    int status = quanta_mutex_init(&shared.mutex);
    CHECK(status == 0);
    if (status)
    {
        return;
    }
    status = quanta_condition_init(&shared.ready);
    CHECK(status == 0);
    if (status)
    {
        quanta_mutex_destroy(&shared.mutex);
        return;
    }
    for (unsigned i = 0; i < 2; i++)
    {
        status = quanta_thread_start(&workers[i], worker, &shared);
        CHECK(status == 0);
        if (status)
        {
            break;
        }
        started++;
    }
    quanta_mutex_lock(&shared.mutex);
    while (shared.arrived < started)
    {
        quanta_condition_wait(&shared.ready, &shared.mutex);
    }
    shared.start = 1;
    quanta_condition_broadcast(&shared.ready);
    quanta_mutex_unlock(&shared.mutex);
    for (unsigned i = 0; i < started; i++)
    {
        quanta_thread_join(&workers[i]);
    }
    CHECK(shared.count == started * 5000);
    quanta_condition_destroy(&shared.ready);
    quanta_mutex_destroy(&shared.mutex);
}

static void files_and_time(void)
{
    char path[256], buffer[64], tiny[3], utc[64];
    uint64_t number;
    FILE *file;
    uint64_t before = quanta_now_ns();
    quanta_sleep_ns(QUANTA_NS_MS);
    CHECK(quanta_now_ns() >= before && quanta_realtime_ns() > 0);
    quanta_utc_string(0, utc, sizeof(utc));
    CHECK(strncmp(utc, "1970-01-01", 10) == 0);
    CHECK(quanta_path(buffer, sizeof(buffer), "root", "child"));
    CHECK(!quanta_path(tiny, sizeof(tiny), "root", "child"));
    snprintf(path, sizeof(path), "quanta-osal-%lu-%llu.tmp", quanta_process_id(),
             (unsigned long long)before);
    file = quanta_create_output(path);
    CHECK(file != NULL);
    if (!file)
    {
        return;
    }
    CHECK(fputs("18446744073709551615\n", file) >= 0);
    CHECK(fclose(file) == 0);
    file = quanta_create_output(path);
    CHECK(file == NULL);
    if (file)
    {
        fclose(file);
    }
    CHECK(quanta_read_u64(path, &number) == QUANTA_OK && number == UINT64_MAX);
    CHECK(quanta_read_text(path, tiny, sizeof(tiny)) == QUANTA_LIMIT && !tiny[0]);
    CHECK(quanta_read_text(path, buffer, sizeof(buffer)) == QUANTA_OK && strlen(buffer) == 21);
    file = quanta_fopen_utf8(path, "wb");
    CHECK(file != NULL);
    if (file)
    {
        const char invalid[] = {'a', 0, 'b'};
        CHECK(fwrite(invalid, 1, sizeof(invalid), file) == sizeof(invalid));
        CHECK(fclose(file) == 0);
        CHECK(quanta_read_text(path, buffer, sizeof(buffer)) == QUANTA_INVALID_DATA);
    }
    CHECK(remove(path) == 0);
    CHECK(quanta_read_text(path, buffer, sizeof(buffer)) != QUANTA_OK);
    CHECK(quanta_errno_status(EACCES) == QUANTA_PERMISSION_DENIED);
}

int main(void)
{
    threads();
    files_and_time();
    printf("Native OSAL: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
