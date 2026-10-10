/* SPDX-License-Identifier: MIT */
#include "thread.h"
int inv_mutex_init(inv_mutex *m) { return pthread_mutex_init(m,0); }
void inv_mutex_destroy(inv_mutex *m) { (void)pthread_mutex_destroy(m); }
void inv_mutex_lock(inv_mutex *m) { (void)pthread_mutex_lock(m); }
void inv_mutex_unlock(inv_mutex *m) { (void)pthread_mutex_unlock(m); }
int inv_condition_init(inv_condition *c) { return pthread_cond_init(c,0); }
void inv_condition_destroy(inv_condition *c) { (void)pthread_cond_destroy(c); }
void inv_condition_wait(inv_condition *c,inv_mutex *m) { (void)pthread_cond_wait(c,m); }
void inv_condition_signal(inv_condition *c) { (void)pthread_cond_signal(c); }
void inv_condition_broadcast(inv_condition *c) { (void)pthread_cond_broadcast(c); }
int inv_thread_start(inv_thread *t,void *(*fn)(void *),void *arg) { return pthread_create(&t->handle,0,fn,arg); }
void inv_thread_join(inv_thread *t) { (void)pthread_join(t->handle,0); }
