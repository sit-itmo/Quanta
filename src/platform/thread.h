/* SPDX-License-Identifier: MIT -- internal platform types, not public API */
#ifndef INV_THREAD_H
#define INV_THREAD_H
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
typedef CRITICAL_SECTION inv_mutex;
typedef CONDITION_VARIABLE inv_condition;
typedef struct inv_thread { HANDLE handle; } inv_thread;
#else
#include <pthread.h>
typedef pthread_mutex_t inv_mutex;
typedef pthread_cond_t inv_condition;
typedef struct inv_thread { pthread_t handle; } inv_thread;
#endif
int inv_mutex_init(inv_mutex *);
void inv_mutex_destroy(inv_mutex *);
void inv_mutex_lock(inv_mutex *);
void inv_mutex_unlock(inv_mutex *);
int inv_condition_init(inv_condition *);
void inv_condition_destroy(inv_condition *);
void inv_condition_wait(inv_condition *,inv_mutex *);
void inv_condition_signal(inv_condition *);
void inv_condition_broadcast(inv_condition *);
int inv_thread_start(inv_thread *,void *(*)(void *),void *);
void inv_thread_join(inv_thread *);
#endif
