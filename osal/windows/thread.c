/* SPDX-License-Identifier: MIT */
#include "thread.h"
#include <process.h>
#include <stdlib.h>

typedef struct start_info
{
    void *(*fn)(void *);
    void *arg;
} start_info;

static unsigned __stdcall entry(void *arg)
{
    start_info data = *(start_info *)arg;
    free(arg);
    (void)data.fn(data.arg);
    return 0;
}

int quanta_mutex_init(quanta_mutex *m)
{
    return InitializeCriticalSectionEx(m, 0, 0) ? 0 : 1;
}

void quanta_mutex_destroy(quanta_mutex *m)
{
    DeleteCriticalSection(m);
}

void quanta_mutex_lock(quanta_mutex *m)
{
    EnterCriticalSection(m);
}

void quanta_mutex_unlock(quanta_mutex *m)
{
    LeaveCriticalSection(m);
}

int quanta_condition_init(quanta_condition *c)
{
    InitializeConditionVariable(c);
    return 0;
}

void quanta_condition_destroy(quanta_condition *c)
{
    (void)c;
}

void quanta_condition_wait(quanta_condition *c, quanta_mutex *m)
{
    /* Infinite wait cannot time out. Spurious wakes are handled by the caller. */
    (void)SleepConditionVariableCS(c, m, INFINITE);
}

void quanta_condition_signal(quanta_condition *c)
{
    WakeConditionVariable(c);
}

void quanta_condition_broadcast(quanta_condition *c)
{
    WakeAllConditionVariable(c);
}

int quanta_thread_start(quanta_thread *t, void *(*fn)(void *), void *arg)
{
    start_info *s = (start_info *)malloc(sizeof(*s));
    if (!s)
    {
        return 1;
    }
    s->fn = fn;
    s->arg = arg;
    t->handle = (HANDLE)_beginthreadex(NULL, 0, entry, s, 0, NULL);
    if (!t->handle)
    {
        free(s);
        return 1;
    }
    return 0;
}

void quanta_thread_join(quanta_thread *t)
{
    if (t->handle)
    {
        (void)WaitForSingleObject(t->handle, INFINITE);
        CloseHandle(t->handle);
        t->handle = NULL;
    }
}
