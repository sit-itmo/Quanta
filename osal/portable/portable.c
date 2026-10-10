/**
 * @file osal/portable/portable.c
 *
 * Minimal POSIX runtime without native collectors or UI; unsupported conveniences are reported
 * explicitly.
 *
 * OSAL: the minimal runtime contract for memory, files, clocks, threads and synchronization. OS
 * metrics belong to sensors, not OSAL.
 */
/* SPDX-License-Identifier: MIT
 * Reference minimum POSIX port. No Linux syscalls, signals, process enumeration,
 * procfs/sysfs, file descriptors or desktop facilities. */
#include <locale.h>
#include "quanta/osal.h"
#include <time.h>

uint64_t quanta_now_ns(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time)) return 0;
    return (uint64_t)time.tv_sec * QUANTA_NS_S + (uint64_t)time.tv_nsec;
}
uint64_t quanta_realtime_ns(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_REALTIME, &time)) return 0;
    return (uint64_t)time.tv_sec * QUANTA_NS_S + (uint64_t)time.tv_nsec;
}
void quanta_sleep_ns(uint64_t nanoseconds)
{
    struct timespec time;
    time.tv_sec = (time_t)(nanoseconds / QUANTA_NS_S);
    time.tv_nsec = (long)(nanoseconds % QUANTA_NS_S);
    while (nanosleep(&time, &time) && quanta_errno == EINTR) { }
}
quanta_file *quanta_fopen(const char *path, const char *mode)
{ return fopen(path, mode); }
quanta_file *quanta_create_output(const char *path)
{ return fopen(path, "wbx"); }
int quanta_install_signal_handlers(void) { return 1; }
int quanta_interrupted(void) { return 0; }
unsigned long quanta_process_id(void) { return 0; }

/* Application-only locale policy; never changed by the core or worker thread. */
int quanta_numeric_locale(void)
{
    return setlocale(LC_NUMERIC, "C") != NULL;
}
