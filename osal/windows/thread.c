/**
 * @file osal/windows/thread.c
 *
 * Hidden Windows implementation of threads, locks and condition variables with resource
 * ownership management.
 *
 * OSAL: the minimal runtime contract for memory, files, clocks, threads and synchronization. OS
 * metrics belong to sensors, not OSAL.
 *
 * Event: an internal queue record delivered from the worker to a consumer: a series change,
 * sample, frame or diagnostic. This is not an OS event subscription.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include <windows.h>
#include <process.h>

typedef struct thread_start
{
    quanta_thread_fn function;
    void *argument;
} thread_start;

static unsigned __stdcall thread_entry(void *argument)
{
    thread_start start = *(thread_start *)argument;
    quanta_free(argument);
    (void)start.function(start.argument);
    return 0;
}

int quanta_mutex_init(quanta_mutex *mutex)
{
    SRWLOCK *native;
    if (!mutex) return EINVAL;
    mutex->handle = NULL;
    native = (SRWLOCK *)quanta_malloc(sizeof(*native));
    if (!native) return ENOMEM;
    InitializeSRWLock(native);
    mutex->handle = native;
    return 0;
}
void quanta_mutex_destroy(quanta_mutex *mutex)
{
    if (mutex) { quanta_free(mutex->handle); mutex->handle = NULL; }
}
void quanta_mutex_lock(quanta_mutex *mutex)
{ AcquireSRWLockExclusive((SRWLOCK *)mutex->handle); }
void quanta_mutex_unlock(quanta_mutex *mutex)
{ ReleaseSRWLockExclusive((SRWLOCK *)mutex->handle); }

int quanta_condition_init(quanta_condition *condition)
{
    CONDITION_VARIABLE *native;
    if (!condition) return EINVAL;
    condition->handle = NULL;
    native = (CONDITION_VARIABLE *)quanta_malloc(sizeof(*native));
    if (!native) return ENOMEM;
    InitializeConditionVariable(native);
    condition->handle = native;
    return 0;
}
void quanta_condition_destroy(quanta_condition *condition)
{
    if (condition) { quanta_free(condition->handle); condition->handle = NULL; }
}
void quanta_condition_wait(quanta_condition *condition, quanta_mutex *mutex)
{ (void)SleepConditionVariableSRW((CONDITION_VARIABLE *)condition->handle,
                                (SRWLOCK *)mutex->handle, INFINITE, 0); }
void quanta_condition_signal(quanta_condition *condition)
{ WakeConditionVariable((CONDITION_VARIABLE *)condition->handle); }
void quanta_condition_broadcast(quanta_condition *condition)
{ WakeAllConditionVariable((CONDITION_VARIABLE *)condition->handle); }

int quanta_thread_start(quanta_thread *thread, quanta_thread_fn function, void *argument)
{
    thread_start *start;
    HANDLE handle;
    if (!thread) return EINVAL;
    thread->handle = NULL;
    if (!function) return EINVAL;
    start = (thread_start *)quanta_malloc(sizeof(*start));
    if (!start) return ENOMEM;
    start->function = function;
    start->argument = argument;
    handle = (HANDLE)_beginthreadex(NULL, 0, thread_entry, start, 0, NULL);
    if (!handle)
    {
        quanta_free(start);
        return EAGAIN;
    }
    thread->handle = handle;
    return 0;
}
void quanta_thread_join(quanta_thread *thread)
{
    if (thread && thread->handle)
    {
        (void)WaitForSingleObject((HANDLE)thread->handle, INFINITE);
        CloseHandle((HANDLE)thread->handle);
        thread->handle = NULL;
    }
}
