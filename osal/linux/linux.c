/**
 * @file osal/linux/linux.c
 *
 * Linux runtime: monotonic/calendar clocks, sleep, signals, files and numeric locale.
 *
 * OSAL: the minimal runtime contract for memory, files, clocks, threads and synchronization. OS
 * metrics belong to sensors, not OSAL.
 */
/* SPDX-License-Identifier: MIT */
#include <locale.h>
#include "support.h"
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
static volatile sig_atomic_t interrupted = 0;

static void signal_handler(int n)
{
    interrupted = n;
}

int quanta_install_signal_handlers(void)
{
    struct sigaction a;
    memset(&a, 0, sizeof(a));
    a.sa_handler = signal_handler;
    sigemptyset(&a.sa_mask);
    if (sigaction(SIGINT, &a, NULL) || sigaction(SIGTERM, &a, NULL) || sigaction(SIGHUP, &a, NULL))
    {
        return 0;
    }
    a.sa_handler = SIG_IGN;
    return sigaction(SIGPIPE, &a, NULL) == 0;
}

int quanta_interrupted(void)
{
    return (int)interrupted;
}

uint64_t quanta_now_ns(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t))
    {
        return 0;
    }
    return (uint64_t)t.tv_sec * QUANTA_NS_S + (uint64_t)t.tv_nsec;
}

uint64_t quanta_realtime_ns(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_REALTIME, &t))
    {
        return 0;
    }
    return (uint64_t)t.tv_sec * QUANTA_NS_S + (uint64_t)t.tv_nsec;
}

void quanta_sleep_ns(uint64_t ns)
{
    struct timespec t;
    t.tv_sec = (time_t)(ns / QUANTA_NS_S);
    t.tv_nsec = (long)(ns % QUANTA_NS_S);
    while (nanosleep(&t, &t) && errno == EINTR && !interrupted)
    {
    }
}

FILE *quanta_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

FILE *quanta_create_output(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    FILE *f;
    if (fd < 0)
    {
        return NULL;
    }
    f = fdopen(fd, "w");
    if (!f)
    {
        int saved = errno;
        close(fd);
        errno = saved;
    }
    return f;
}

unsigned long quanta_process_id(void)
{
    return (unsigned long)getpid();
}

/* Application-only locale policy; never changed by the core or worker thread. */
int quanta_numeric_locale(void)
{
    return setlocale(LC_NUMERIC, "C") != NULL;
}
