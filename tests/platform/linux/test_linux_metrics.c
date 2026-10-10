/**
 * @file tests/platform/linux/test_linux_metrics.c
 *
 * Regression tests for Linux native-data parsing and value normalization.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
/* SPDX-License-Identifier: MIT -- deterministic native parsers, no hardware
 * writes */
#include "quanta/osal.h"
#include "cpu.h"
#include "snapshot_context.h"
#include "memory.h"
#include "os.h"
#include "process.h"
#include "support.h"
#include <sys/stat.h>
#include <unistd.h>

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

static void put(const char *root, const char *name, const char *text)
{
    char path[QUANTA_PATH_CAP];
    quanta_file *file;
    CHECK(quanta_path(path, sizeof(path), root, name));
    file = fopen(path, "w");
    CHECK(file != NULL);
    if (file)
    {
        CHECK(quanta_fputs(text, file) >= 0);
        CHECK(quanta_fclose(file) == 0);
    }
}

static void erase(const char *root, const char *name)
{
    char path[QUANTA_PATH_CAP];
    CHECK(quanta_path(path, sizeof(path), root, name));
    CHECK(unlink(path) == 0);
}

int main(void)
{
    char root[] = "/tmp/quanta-hal-XXXXXX";
    char path[QUANTA_PATH_CAP];
    quanta_snapshot_context context;
    quanta_os_snapshot os;
    CHECK(mkdtemp(root) != NULL);
    quanta_memset(&context, 0, sizeof(context));
    context.paths.proc_root = context.paths.etc_root = context.paths.sys_root = root;
    put(root, "os-release", "NAME=unused\nPRETTY_NAME=\"Fixture OS $(not a command)\"\n");
    put(root, "uptime", "123.5 456.5\n");
    put(root, "loadavg", "1.25 2.5 3.75 2/100 42\n");
    CHECK(quanta_native_os_refresh(&os, context) == QUANTA_OK);
    CHECK(os.kernel_status == QUANTA_OK && os.arch[0] && os.kernel[0]);
    CHECK(!quanta_strcmp(os.name, "Fixture OS $(not a command)"));
    CHECK(os.uptime_status == QUANTA_OK && os.uptime == 123.5);
    CHECK(os.load_status == QUANTA_OK && os.load[2] == 3.75);
    put(root, "uptime", "nan\n");
    put(root, "loadavg", "1.0bad 2.0 3.0\n");
    CHECK(quanta_native_os_refresh(&os, context) == QUANTA_OK);
    CHECK(os.uptime_status == QUANTA_INVALID_DATA && os.load_status == QUANTA_INVALID_DATA);
    erase(root, "os-release");
    erase(root, "uptime");
    erase(root, "loadavg");
#if QUANTA_WITH_CPU
    {
        quanta_cpu_snapshot cpu;
        quanta_memset(&cpu, 0, sizeof(cpu));
        put(root, "stat", "cpu 10 20 30 40 5 6 7 8 100 200\ncpu0 1 2 3 4\nintr 123\n");
        CHECK(quanta_native_cpu_refresh(&cpu, context) == QUANTA_OK);
        CHECK(cpu.count == 2 && cpu.rows[0].t[0] == 10 && cpu.rows[0].t[7] == 8);
        CHECK(cpu.tick_frequency_hz == (uint64_t)sysconf(_SC_CLK_TCK));
        put(root, "stat", "cpu 1 2 3 4\ncpu0 1 2 3 4\ncpu0 1 2 3 4\n");
        CHECK(quanta_native_cpu_refresh(&cpu, context) == QUANTA_INVALID_DATA);
        put(root, "stat", "cpu 18446744073709551616 2 3 4\n");
        CHECK(quanta_native_cpu_refresh(&cpu, context) == QUANTA_INVALID_DATA);
        put(root, "stat", "cpu 1 2 3bad 4\n");
        CHECK(quanta_native_cpu_refresh(&cpu, context) == QUANTA_INVALID_DATA);
        quanta_native_cpu_cleanup(&cpu);
        quanta_native_cpu_cleanup(&cpu);
        CHECK(cpu.rows == NULL && cpu.count == 0);
        erase(root, "stat");
    }
#endif
#if QUANTA_WITH_MEMORY
    {
        quanta_linux_meminfo memory;
        CHECK(quanta_native_parse_meminfo(NULL, &memory) == QUANTA_INVALID_DATA);
        CHECK(quanta_native_parse_meminfo("MemTotal: 10 kB\n", NULL) == QUANTA_INVALID_DATA);
        CHECK(quanta_native_parse_meminfo("ignored line\nMemTotal: 10 kB\nMemFree: 2 kB\n", &memory) ==
              QUANTA_OK);
        CHECK(memory.total == 10240 && memory.free == 2048);
        CHECK(quanta_native_parse_meminfo("MemTotal: 10 kBgarbage\n", &memory) == QUANTA_INVALID_DATA);
        CHECK(quanta_native_parse_meminfo("MemTotal: 10kB\n", &memory) == QUANTA_INVALID_DATA);
        CHECK(quanta_native_parse_meminfo("MemTotal: 10 kB\nMemTotal: 11 kB\n", &memory) ==
              QUANTA_INVALID_DATA);
        CHECK(quanta_native_parse_meminfo("MemTotal: 10 kB\nMemFree: 11 kB\n", &memory) ==
              QUANTA_INVALID_DATA);
        CHECK(quanta_native_parse_meminfo("MemTotal: 18446744073709551615 kB\n", &memory) ==
              QUANTA_INVALID_DATA);
    }
#endif
#if QUANTA_WITH_PROCESS
    {
        quanta_process_snapshot process;
        CHECK(quanta_path(path, sizeof(path), root, "self"));
        CHECK(mkdir(path, 0700) == 0);
        put(root, "self/statm", "100 7 3 0 0 0 0\n");
        CHECK(quanta_native_process_refresh(&process, context) == QUANTA_OK);
        CHECK(process.memory_status == QUANTA_OK && process.pid == (uint64_t)getpid());
        CHECK(process.rss_bytes == 7 * (uint64_t)sysconf(_SC_PAGESIZE));
        put(root, "self/statm", "100 -7\n");
        CHECK(quanta_native_process_refresh(&process, context) == QUANTA_OK);
        CHECK(process.memory_status == QUANTA_INVALID_DATA);
        erase(root, "self/statm");
        CHECK(rmdir(path) == 0);
    }
#else
    (void)path;
#endif
    CHECK(rmdir(root) == 0);
    quanta_printf("Linux HAL fixtures: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
