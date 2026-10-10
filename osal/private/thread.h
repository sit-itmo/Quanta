/* SPDX-License-Identifier: MIT -- internal platform types, not public API */
#ifndef QUANTA_THREAD_H
#define QUANTA_THREAD_H
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
typedef CRITICAL_SECTION quanta_mutex;
typedef CONDITION_VARIABLE quanta_condition;

typedef struct quanta_thread
{
    HANDLE handle;
} quanta_thread;
#else
#include <pthread.h>
typedef pthread_mutex_t quanta_mutex;
typedef pthread_cond_t quanta_condition;

typedef struct quanta_thread
{
    pthread_t handle;
} quanta_thread;
#endif
int quanta_mutex_init(quanta_mutex *);
void quanta_mutex_destroy(quanta_mutex *);
void quanta_mutex_lock(quanta_mutex *);
void quanta_mutex_unlock(quanta_mutex *);
int quanta_condition_init(quanta_condition *);
void quanta_condition_destroy(quanta_condition *);
void quanta_condition_wait(quanta_condition *, quanta_mutex *);
void quanta_condition_signal(quanta_condition *);
void quanta_condition_broadcast(quanta_condition *);
int quanta_thread_start(quanta_thread *, void *(*)(void *), void *);
void quanta_thread_join(quanta_thread *);
#endif
