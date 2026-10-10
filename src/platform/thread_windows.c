/* SPDX-License-Identifier: MIT */
#include "thread.h"
#include <process.h>
#include <stdlib.h>
typedef struct start_info { void *(*fn)(void *); void *arg; } start_info;
static unsigned __stdcall entry(void *arg) {
    start_info data=*(start_info *)arg;
    free(arg);
    (void)data.fn(data.arg);
    return 0;
}
int inv_mutex_init(inv_mutex *m) { return InitializeCriticalSectionEx(m,0,0)?0:1; }
void inv_mutex_destroy(inv_mutex *m) { DeleteCriticalSection(m); }
void inv_mutex_lock(inv_mutex *m) { EnterCriticalSection(m); }
void inv_mutex_unlock(inv_mutex *m) { LeaveCriticalSection(m); }
int inv_condition_init(inv_condition *c) { InitializeConditionVariable(c); return 0; }
void inv_condition_destroy(inv_condition *c) { (void)c; }
void inv_condition_wait(inv_condition *c,inv_mutex *m) {
    /* Infinite wait cannot time out. Spurious wakes are handled by the caller. */
    (void)SleepConditionVariableCS(c,m,INFINITE);
}
void inv_condition_signal(inv_condition *c) { WakeConditionVariable(c); }
void inv_condition_broadcast(inv_condition *c) { WakeAllConditionVariable(c); }
int inv_thread_start(inv_thread *t,void *(*fn)(void *),void *arg) {
    start_info *s=(start_info *)malloc(sizeof(*s));
    if(!s)return 1;
    s->fn=fn; s->arg=arg;
    t->handle=(HANDLE)_beginthreadex(NULL,0,entry,s,0,NULL);
    if(!t->handle) { free(s); return 1; }
    return 0;
}
void inv_thread_join(inv_thread *t) {
    if(t->handle) { (void)WaitForSingleObject(t->handle,INFINITE); CloseHandle(t->handle); t->handle=NULL; }
}
