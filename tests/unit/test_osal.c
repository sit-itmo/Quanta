/* SPDX-License-Identifier: MIT -- exercise the real native OSAL behind its
 * abstraction. */
#include "quanta/osal.h"
#include "quanta/quanta.h"
static unsigned checks, failures;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        ++checks;                                                                                  \
        if (!(x))                                                                                  \
        {                                                                                          \
            ++failures;                                                                            \
            quanta_fprintf(quanta_stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                \
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
    CHECK(!shared.ready.handle && !shared.mutex.handle);
    for (unsigned i = 0; i < started; ++i)
    {
        CHECK(!workers[i].handle);
        quanta_thread_join(&workers[i]);
    }
    quanta_condition_destroy(&shared.ready);
    quanta_mutex_destroy(&shared.mutex);
}

static void files_and_time(void)
{
    char path[256], buffer[64], tiny[3], utc[64];
    uint64_t number;
    quanta_file *file;
    uint64_t before = quanta_now_ns();
    quanta_sleep_ns(QUANTA_NS_MS);
    CHECK(quanta_now_ns() >= before && quanta_realtime_ns() > 0);
    quanta_utc_string(0, utc, sizeof(utc));
    CHECK(quanta_strncmp(utc, "1970-01-01", 10) == 0);
    CHECK(quanta_path(buffer, sizeof(buffer), "root", "child"));
    CHECK(!quanta_path(tiny, sizeof(tiny), "root", "child"));
    quanta_snprintf(path, sizeof(path), "quanta-osal-%lu-%llu.tmp", quanta_process_id(),
             (unsigned long long)before);
    file = quanta_create_output(path);
    CHECK(file != NULL);
    if (!file)
    {
        return;
    }
    CHECK(quanta_fputs("18446744073709551615\n", file) >= 0);
    CHECK(quanta_fclose(file) == 0);
    file = quanta_create_output(path);
    CHECK(file == NULL);
    if (file)
    {
        quanta_fclose(file);
    }
    CHECK(quanta_read_u64(path, &number) == QUANTA_OK && number == UINT64_MAX);
    CHECK(quanta_read_text(path, tiny, sizeof(tiny)) == QUANTA_LIMIT && !tiny[0]);
    CHECK(quanta_read_text(path, buffer, sizeof(buffer)) == QUANTA_OK && quanta_strlen(buffer) == 21);
    file = quanta_fopen(path, "wb");
    CHECK(file != NULL);
    if (file)
    {
        const char invalid[] = {'a', 0, 'b'};
        CHECK(quanta_fwrite(invalid, 1, sizeof(invalid), file) == sizeof(invalid));
        CHECK(quanta_fclose(file) == 0);
        CHECK(quanta_read_text(path, buffer, sizeof(buffer)) == QUANTA_INVALID_DATA);
    }
    CHECK(quanta_remove(path) == 0);
    CHECK(quanta_read_text(path, buffer, sizeof(buffer)) != QUANTA_OK);
    CHECK(quanta_errno_status(EACCES) == QUANTA_PERMISSION_DENIED);
}

static void memory_and_lifecycle(void)
{
    unsigned char *bytes;
    quanta_thread thread;
    CHECK(quanta_mutex_init(NULL) != 0);
    CHECK(quanta_condition_init(NULL) != 0);
    CHECK(quanta_thread_start(NULL, worker, NULL) != 0);
    CHECK(quanta_thread_start(&thread, NULL, NULL) != 0 && !thread.handle);
    quanta_thread_join(&thread);
    quanta_thread_join(NULL);
    quanta_condition_destroy(NULL);
    quanta_mutex_destroy(NULL);
    CHECK(quanta_calloc(SIZE_MAX, 2) == NULL && quanta_errno == ENOMEM);
    bytes = (unsigned char *)quanta_calloc(4, 4);
    CHECK(bytes != NULL);
    if (bytes)
    {
        unsigned char zero[16] = {0};
        unsigned char *larger;
        CHECK(quanta_memcmp(bytes, zero, 16) == 0);
        quanta_memset(bytes, 0xA5, 16);
        larger = (unsigned char *)quanta_realloc(bytes, 32);
        CHECK(larger != NULL);
        if (larger)
        {
            bytes = larger;
            CHECK(bytes[0] == 0xA5 && bytes[15] == 0xA5);
            quanta_memmove(bytes + 1, bytes, 16);
            CHECK(bytes[16] == 0xA5);
        }
        quanta_free(bytes);
    }
    bytes = (unsigned char *)quanta_calloc(0, SIZE_MAX);
    CHECK(bytes != NULL);
    quanta_free(bytes);
    CHECK(quanta_isalpha('Z') && quanta_isdigit('9') && quanta_isspace('\n'));
    CHECK(quanta_tolower('A') == 'a' && !quanta_isalpha(-1) && !quanta_isspace(0xA0));
    CHECK(quanta_numeric_locale());
}

static void stream_contract(void)
{
    char path[160], buffer[16];
    quanta_file *file;
    quanta_snprintf(path, sizeof(path), "quanta-stream-%llu.tmp",
                    (unsigned long long)quanta_now_ns());
    file = quanta_create_output(path);
    CHECK(file != NULL);
    if (!file)
    {
        return;
    }
    /* fread/fwrite preserve standard item counts; they are not byte-only APIs. */
    CHECK(quanta_fwrite("abcdef", 2, 3, file) == 3);
    CHECK(quanta_fflush(file) == 0);
    CHECK(quanta_ftell(file) == 6);
    CHECK(quanta_fclose(file) == 0);
    file = quanta_fopen(path, "rb");
    CHECK(file != NULL);
    if (file)
    {
        CHECK(quanta_fread(buffer, 2, 4, file) == 3);
        CHECK(quanta_memcmp(buffer, "abcdef", 6) == 0);
        CHECK(quanta_feof(file) && !quanta_ferror(file));
        quanta_clearerr(file);
        CHECK(!quanta_feof(file));
        CHECK(quanta_fseek(file, -2, QUANTA_SEEK_END) == 0);
        CHECK(quanta_ftell(file) == 4 && quanta_fgetc(file) == 'e');
        CHECK(quanta_fseek(file, 0, QUANTA_SEEK_SET) == 0);
        CHECK(quanta_fgets(buffer, sizeof(buffer), file) == buffer);
        CHECK(quanta_strcmp(buffer, "abcdef") == 0);
        CHECK(quanta_fclose(file) == 0);
    }
    CHECK(quanta_read_text(path, buffer, 7) == QUANTA_OK && quanta_strlen(buffer) == 6);
    CHECK(quanta_read_text(path, buffer, 6) == QUANTA_LIMIT && !buffer[0]);
    CHECK(quanta_read_text(NULL, buffer, sizeof(buffer)) == QUANTA_INVALID_DATA && !buffer[0]);
    CHECK(quanta_read_text(path, NULL, 0) == QUANTA_INVALID_DATA);
    file = quanta_fopen(path, "wb");
    CHECK(file != NULL);
    if (file)
    {
        CHECK(quanta_fclose(file) == 0);
        CHECK(quanta_read_text(path, buffer, sizeof(buffer)) == QUANTA_OK && !buffer[0]);
    }
    CHECK(quanta_remove(path) == 0);
}

static void utc_vectors(void)
{
    static const struct { uint64_t ns; const char *text; } cases[] = {
        {UINT64_C(0), "1970-01-01T00:00:00.000Z"},
        {UINT64_C(951782400000000000), "2000-02-29T00:00:00.000Z"},
        {UINT64_C(4107542400000000000), "2100-03-01T00:00:00.000Z"},
        {UINT64_MAX, "2554-07-21T23:34:33.709Z"}
    };
    char text[32];
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        quanta_utc_string(cases[i].ns, text, sizeof(text));
        CHECK(quanta_strcmp(text, cases[i].text) == 0);
    }
    quanta_utc_string(0, text, 1);
    CHECK(!text[0]);
    quanta_utc_string(0, NULL, 0);
}

int main(void)
{
    memory_and_lifecycle();
    threads();
    files_and_time();
    stream_contract();
    utc_vectors();
    quanta_printf("Runtime OSAL: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
