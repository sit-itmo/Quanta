/* SPDX-License-Identifier: MIT */
#include "thread.h"

int quanta_mutex_init(quanta_mutex *m)
{
    return pthread_mutex_init(m, 0);
}

void quanta_mutex_destroy(quanta_mutex *m)
{
    (void)pthread_mutex_destroy(m);
}

void quanta_mutex_lock(quanta_mutex *m)
{
    (void)pthread_mutex_lock(m);
}

void quanta_mutex_unlock(quanta_mutex *m)
{
    (void)pthread_mutex_unlock(m);
}

int quanta_condition_init(quanta_condition *c)
{
    return pthread_cond_init(c, 0);
}

void quanta_condition_destroy(quanta_condition *c)
{
    (void)pthread_cond_destroy(c);
}

void quanta_condition_wait(quanta_condition *c, quanta_mutex *m)
{
    (void)pthread_cond_wait(c, m);
}

void quanta_condition_signal(quanta_condition *c)
{
    (void)pthread_cond_signal(c);
}

void quanta_condition_broadcast(quanta_condition *c)
{
    (void)pthread_cond_broadcast(c);
}

int quanta_thread_start(quanta_thread *t, void *(*fn)(void *), void *arg)
{
    return pthread_create(&t->handle, 0, fn, arg);
}

void quanta_thread_join(quanta_thread *t)
{
    (void)pthread_join(t->handle, 0);
}
