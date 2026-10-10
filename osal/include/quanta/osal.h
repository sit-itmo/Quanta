/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_OSAL_H
#define QUANTA_OSAL_H
#include "quanta/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
uint64_t quanta_now_ns(void);
uint64_t quanta_realtime_ns(void);
void quanta_sleep_ns(uint64_t nanoseconds);
int quanta_install_signal_handlers(void);
int quanta_interrupted(void);
FILE *quanta_fopen_utf8(const char *path, const char *mode);
FILE *quanta_create_output(const char *path);
unsigned long quanta_process_id(void);
quanta_status quanta_errno_status(int error);
quanta_status quanta_read_text(const char *path, char *buffer, size_t capacity);
quanta_status quanta_read_u64(const char *path, uint64_t *value);
int quanta_path(char *buffer, size_t capacity, const char *root, const char *relative);
void quanta_utc_string(uint64_t realtime_ns, char *buffer, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
