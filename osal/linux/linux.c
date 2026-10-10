/* SPDX-License-Identifier: MIT */
#include "osal_internal.h"
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

int quanta_path(char *out, size_t cap, const char *root, const char *rel)
{
    int n = snprintf(out, cap, "%s/%s", root, rel);
    return n >= 0 && (size_t)n < cap;
}

quanta_status quanta_read_text(const char *path, char *out, size_t cap)
{
    int fd;
    size_t n = 0;
    ssize_t r;
    char extra;
    quanta_status s = QUANTA_OK;
    if (cap < 2)
    {
        return QUANTA_LIMIT;
    }
    out[0] = 0;
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        return quanta_errno_status(errno);
    }
    while (n + 1 < cap)
    {
        r = read(fd, out + n, cap - 1 - n);
        if (r > 0)
        {
            n += (size_t)r;
            continue;
        }
        if (r < 0 && errno == EINTR)
        {
            continue;
        }
        if (r < 0)
        {
            s = quanta_errno_status(errno);
        }
        break;
    }
    if (s == QUANTA_OK && n + 1 == cap)
    {
        do
        {
            r = read(fd, &extra, 1);
        } while (r < 0 && errno == EINTR);
        if (r > 0)
        {
            s = QUANTA_LIMIT;
        }
        else if (r < 0)
        {
            s = quanta_errno_status(errno);
        }
    }
    close(fd);
    if (s == QUANTA_OK && memchr(out, 0, n))
    {
        s = QUANTA_INVALID_DATA;
    }
    out[n] = 0;
    if (s != QUANTA_OK)
    {
        out[0] = 0;
    }
    return s;
}

quanta_status quanta_read_u64(const char *p, uint64_t *v)
{
    char b[128];
    quanta_status s = quanta_read_text(p, b, sizeof(b));
    return s != QUANTA_OK ? s : (quanta_parse_u64(b, v) ? QUANTA_OK : QUANTA_INVALID_DATA);
}

FILE *quanta_fopen_utf8(const char *path, const char *mode)
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
