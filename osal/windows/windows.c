/**
 * @file osal/windows/windows.c
 *
 * Windows runtime: clocks, sleep, UTF-8/UTF-16 paths, output files, signals and error statuses.
 *
 * OSAL: the minimal runtime contract for memory, files, clocks, threads and synchronization. OS
 * metrics belong to sensors, not OSAL.
 */
/* SPDX-License-Identifier: MIT */
#include <locale.h>
#include "windows_internal.h"
#include <signal.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <share.h>
#include <sys/stat.h>
#include <time.h>
static volatile LONG interrupted;
static LARGE_INTEGER frequency;

static BOOL WINAPI control_handler(DWORD event)
{
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT)
    {
        InterlockedExchange(&interrupted, SIGINT);
        return TRUE;
    }
    /* Windows can terminate a console on CLOSE/LOGOFF; never promise cleanup
     * then. */
    return FALSE;
}

static void signal_handler(int sig)
{
    InterlockedExchange(&interrupted, sig);
}

int quanta_install_signal_handlers(void)
{
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
    {
        return 0;
    }
    if (signal(SIGINT, signal_handler) == SIG_ERR || signal(SIGTERM, signal_handler) == SIG_ERR)
    {
        return 0;
    }
    return SetConsoleCtrlHandler(control_handler, TRUE) != 0;
}

int quanta_interrupted(void)
{
    return (int)InterlockedCompareExchange(&interrupted, 0, 0);
}

uint64_t quanta_now_ns(void)
{
    LARGE_INTEGER t, f;
    uint64_t value, divisor;
    /* Safe also for library users that did not install process signal handlers.
     */
    f = frequency;
    if (!f.QuadPart && !QueryPerformanceFrequency(&f))
    {
        return 0;
    }
    if (f.QuadPart <= 0 || !QueryPerformanceCounter(&t) || t.QuadPart < 0)
    {
        return 0;
    }
    value = (uint64_t)t.QuadPart;
    divisor = (uint64_t)f.QuadPart;
    /* Quotient first: QPC ticks * 1e9 would overflow after a long uptime. */
    return value / divisor * QUANTA_NS_S +
           (uint64_t)((double)(value % divisor) * (double)QUANTA_NS_S / (double)divisor);
}

uint64_t quanta_win_filetime(FILETIME ft)
{
    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

uint64_t quanta_realtime_ns(void)
{
    FILETIME ft;
    uint64_t ticks;
    GetSystemTimeAsFileTime(&ft);
    ticks = quanta_win_filetime(ft);
    if (ticks < UINT64_C(116444736000000000))
    {
        return 0;
    }
    ticks -= UINT64_C(116444736000000000);
    return ticks > UINT64_MAX / 100 ? UINT64_MAX : ticks * 100;
}

void quanta_sleep_ns(uint64_t ns)
{
    /* Small chunks keep Ctrl+C and UI cancellation responsive; no
     * timeBeginPeriod. */
    while (ns && !quanta_interrupted())
    {
        DWORD ms = (DWORD)QUANTA_MIN(ns / QUANTA_NS_MS + (ns % QUANTA_NS_MS != 0), UINT64_C(20));
        uint64_t start = quanta_now_ns(), elapsed;
        Sleep(ms);
        elapsed = quanta_now_ns() - start;
        if (!elapsed || elapsed >= ns)
        {
            break;
        }
        ns -= elapsed;
    }
}

quanta_status quanta_win_status(DWORD code)
{
    switch (code)
    {
        case ERROR_SUCCESS:
            return QUANTA_OK;
        case ERROR_ACCESS_DENIED:
        case ERROR_PRIVILEGE_NOT_HELD:
            return QUANTA_PERMISSION_DENIED;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_NOT_FOUND:
        case ERROR_DEV_NOT_EXIST:
        case ERROR_NOT_READY:
            return QUANTA_NOT_PRESENT;
        case ERROR_NOT_SUPPORTED:
        case ERROR_CALL_NOT_IMPLEMENTED:
            return QUANTA_UNSUPPORTED;
        case ERROR_NO_DATA:
            return QUANTA_NO_DATA;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return QUANTA_NO_MEMORY;
        case ERROR_INSUFFICIENT_BUFFER:
        case ERROR_BUFFER_OVERFLOW:
        case ERROR_MORE_DATA:
            return QUANTA_LIMIT;
        case ERROR_TIMEOUT:
        case WAIT_TIMEOUT:
            return QUANTA_TIMEOUT;
        case ERROR_INVALID_DATA:
        case ERROR_BAD_FORMAT:
            return QUANTA_INVALID_DATA;
        case ERROR_OPERATION_ABORTED:
            return QUANTA_CANCELLED;
        default:
            return QUANTA_IO_ERROR;
    }
}

void quanta_win_error(char *buffer, size_t capacity, DWORD code, const char *operation)
{
    snprintf(buffer, capacity, "%s: Windows error %lu", operation, (unsigned long)code);
}

int quanta_win_utf8(char *dst, size_t cap, const wchar_t *s)
{
    int n;
    if (!cap || cap > (size_t)INT_MAX)
    {
        return 0;
    }
    dst[0] = 0;
    n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s, -1, dst, (int)cap, NULL, NULL);
    if (n <= 0)
    {
        dst[0] = 0;
        return 0;
    }
    return 1;
}

wchar_t *quanta_win_widen(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    wchar_t *w;
    if (n <= 0)
    {
        errno = EINVAL;
        return NULL;
    }
    w = (wchar_t *)malloc((size_t)n * sizeof(*w));
    if (!w)
    {
        errno = ENOMEM;
        return NULL;
    }
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, w, n))
    {
        free(w);
        errno = EINVAL;
        return NULL;
    }
    return w;
}

FILE *quanta_fopen(const char *path, const char *mode)
{
    wchar_t *p = quanta_win_widen(path), *m;
    FILE *f;
    if (!p)
    {
        return NULL;
    }
    m = quanta_win_widen(mode);
    if (!m)
    {
        free(p);
        return NULL;
    }
    f = _wfopen(p, m);
    free(m);
    free(p);
    return f;
}

FILE *quanta_create_output(const char *path)
{
    wchar_t *wide = quanta_win_widen(path);
    FILE *f;
    int fd = -1;
    errno_t err;
    if (!wide)
    {
        return NULL;
    }
    err = _wsopen_s(&fd, wide, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT,
                    _SH_DENYWR, _S_IREAD | _S_IWRITE);
    free(wide);
    if (err || fd < 0)
    {
        errno = (int)err;
        return NULL;
    }
    f = _fdopen(fd, "wb");
    if (!f)
    {
        int saved = errno;
        _close(fd);
        errno = saved;
    }
    return f;
}

unsigned long quanta_process_id(void)
{
    return (unsigned long)GetCurrentProcessId();
}


/* Application-only locale policy; never changed by the core or worker thread. */
int quanta_numeric_locale(void)
{
    return setlocale(LC_NUMERIC, "C") != NULL;
}
