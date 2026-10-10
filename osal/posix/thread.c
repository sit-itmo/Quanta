/**
 * @file osal/posix/thread.c
 *
 * Hidden pthread implementations of threads, mutexes and condition variables for POSIX and a
 * pthread-compatible SDK.
 *
 * OSAL: the minimal runtime contract for memory, files, clocks, threads and synchronization. OS
 * metrics belong to sensors, not OSAL.
 *
 * Event: an internal queue record delivered from the worker to a consumer: a series change,
 * sample, frame or diagnostic. This is not an OS event subscription.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include <pthread.h>

int quanta_mutex_init(quanta_mutex *mutex)
{
    pthread_mutex_t *native;
    int status;
    if (!mutex) return EINVAL;
    mutex->handle = NULL;
    native = (pthread_mutex_t *)quanta_malloc(sizeof(*native));
    if (!native) return ENOMEM;
    status = pthread_mutex_init(native, NULL);
    if (status) quanta_free(native);
    else mutex->handle = native;
    return status;
}

void quanta_mutex_destroy(quanta_mutex *mutex)
{
    if (mutex && mutex->handle)
    {
        (void)pthread_mutex_destroy((pthread_mutex_t *)mutex->handle);
        quanta_free(mutex->handle);
        mutex->handle = NULL;
    }
}

void quanta_mutex_lock(quanta_mutex *mutex)
{ (void)pthread_mutex_lock((pthread_mutex_t *)mutex->handle); }
void quanta_mutex_unlock(quanta_mutex *mutex)
{ (void)pthread_mutex_unlock((pthread_mutex_t *)mutex->handle); }

int quanta_condition_init(quanta_condition *condition)
{
    pthread_cond_t *native;
    int status;
    if (!condition) return EINVAL;
    condition->handle = NULL;
    native = (pthread_cond_t *)quanta_malloc(sizeof(*native));
    if (!native) return ENOMEM;
    status = pthread_cond_init(native, NULL);
    if (status) quanta_free(native);
    else condition->handle = native;
    return status;
}

void quanta_condition_destroy(quanta_condition *condition)
{
    if (condition && condition->handle)
    {
        (void)pthread_cond_destroy((pthread_cond_t *)condition->handle);
        quanta_free(condition->handle);
        condition->handle = NULL;
    }
}

void quanta_condition_wait(quanta_condition *condition, quanta_mutex *mutex)
{ (void)pthread_cond_wait((pthread_cond_t *)condition->handle, (pthread_mutex_t *)mutex->handle); }
void quanta_condition_signal(quanta_condition *condition)
{ (void)pthread_cond_signal((pthread_cond_t *)condition->handle); }
void quanta_condition_broadcast(quanta_condition *condition)
{ (void)pthread_cond_broadcast((pthread_cond_t *)condition->handle); }

int quanta_thread_start(quanta_thread *thread, quanta_thread_fn function, void *argument)
{
    pthread_t *native;
    int status;
    if (!thread) return EINVAL;
    thread->handle = NULL;
    if (!function) return EINVAL;
    native = (pthread_t *)quanta_malloc(sizeof(*native));
    if (!native) return ENOMEM;
    status = pthread_create(native, NULL, function, argument);
    if (status) quanta_free(native);
    else thread->handle = native;
    return status;
}

void quanta_thread_join(quanta_thread *thread)
{
    if (thread && thread->handle)
    {
        (void)pthread_join(*(pthread_t *)thread->handle, NULL);
        quanta_free(thread->handle);
        thread->handle = NULL;
    }
}
