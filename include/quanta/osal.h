/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_OSAL_H
#define QUANTA_OSAL_H

#include "quanta/types.h"
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The complete portable runtime contract.
 *
 * 1. C11 scalar types, size_t, uint64_t, double and va_list.
 * 2. The C library aliases below: allocation, byte/string operations, bounded
 *    formatting/parsing, streams and elementary floating-point operations.
 *    No descriptors, directories, mmap, processes, sockets or terminal APIs.
 * 3. The small out-of-line time/thread/file interface declared below.
 *
 * Allocation and independent stream operations must be thread-safe; errno is
 * thread-local. Required error names for the supplied common/portable code:
 * EACCES, EPERM, ENOENT, ENODEV, ENXIO, ENOMEM, EINVAL, EINTR, EAGAIN, ENOSYS,
 * ERANGE. ENODATA, ETIMEDOUT and EOPNOTSUPP are used only when defined.
 * The aliases preserve the C library's arguments, return values and errno.
 * In particular fread/fwrite count ITEMS, not bytes; fseek uses a long offset.
 * Large physical addresses use HAL read_mem, never fseek.
 * snprintf/vsnprintf must have C99 semantics; numeric parsing/printing requires
 * the C numeric locale (the application sets it, an embedding host owns it).
 * NaN/infinity must remain detectable: do not compile with -ffast-math.
 *
 * This is deliberately not a general-purpose POSIX compatibility library.
 * OS-specific sensors/HAL and GUI/TUI backends may use additional native APIs.
 * They do not extend the contract of core or the headless file frontend.
 */

typedef FILE quanta_file;
#define quanta_stdin stdin
#define quanta_stdout stdout
#define quanta_stderr stderr
#define quanta_errno errno
#define QUANTA_EOF EOF
#define QUANTA_SEEK_SET SEEK_SET
#define QUANTA_SEEK_CUR SEEK_CUR
#define QUANTA_SEEK_END SEEK_END

/* Allocation: realloc failure leaves the original allocation owned by caller.
 * calloc is implemented below from malloc + memset with checked multiplication. */
#define quanta_malloc malloc
#define quanta_realloc realloc
#define quanta_free free
#define quanta_memset memset
#define quanta_memcpy memcpy
#define quanta_memmove memmove
#define quanta_memcmp memcmp
#define quanta_memchr memchr

#define quanta_strlen strlen
#define quanta_strcmp strcmp
#define quanta_strncmp strncmp
#define quanta_strchr strchr
#define quanta_strrchr strrchr
#define quanta_strstr strstr
#define quanta_strcpy strcpy
#define quanta_strtol strtol
#define quanta_strtoll strtoll
#define quanta_strtoull strtoull
#define quanta_strtod strtod
#define quanta_strerror strerror

#define quanta_snprintf snprintf
#define quanta_vsnprintf vsnprintf
#define quanta_printf printf
#define quanta_fprintf fprintf
#define quanta_puts puts
#define quanta_fputs fputs
#define quanta_fputc fputc
#define quanta_fgetc fgetc
#define quanta_fgets fgets
#define quanta_fread fread
#define quanta_fwrite fwrite
#define quanta_fseek fseek
#define quanta_ftell ftell
#define quanta_fclose fclose
#define quanta_fflush fflush
#define quanta_ferror ferror
#define quanta_feof feof
#define quanta_clearerr clearerr
#define quanta_remove remove

#define quanta_isfinite isfinite
#define quanta_fabs fabs
#define quanta_floor floor
#define quanta_ceil ceil
#define quanta_pow pow
/* Used by presentation code, not the scheduler. */
#define quanta_qsort qsort
#define quanta_getenv getenv

#ifdef __cplusplus
extern "C" {
#endif

static inline void *quanta_calloc(size_t count, size_t size)
{
    void *p;
    if (size && count > SIZE_MAX / size)
    {
        quanta_errno = ENOMEM;
        return NULL;
    }
    size *= count;
    p = quanta_malloc(size ? size : 1);
    if (p) quanta_memset(p, 0, size);
    return p;
}

/* Query syntax is ASCII regardless of the host locale. These avoid ctype's
 * locale dependency and signed-char preconditions. Unicode text is kept as UTF-8. */
static inline int quanta_isspace(int c)
{ return c == ' ' || (c >= '\t' && c <= '\r'); }
static inline int quanta_isdigit(int c)
{ return c >= '0' && c <= '9'; }
static inline int quanta_isalpha(int c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline int quanta_isalnum(int c)
{ return quanta_isalpha(c) || quanta_isdigit(c); }
static inline int quanta_tolower(int c)
{ return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }

/* Opaque ownership: no windows.h or pthread.h leaks into a public header.
 * init/start accepts an uninitialized object and sets it to empty on failure.
 * A successfully initialized object must not be copied or initialized again.
 * destroy/join releases its resources and resets handle to NULL; repeat cleanup
 * on an empty object is safe. Initialization/start returns 0 on success, nonzero
 * on failure. Locks are NON-RECURSIVE by contract (also on Windows).
 * A condition wait atomically releases the locked mutex and reacquires it before
 * returning. Always wait in a predicate loop: spurious wakes are permitted.
 * Destroy only after all users/waiters have finished. Join only from another
 * thread. No cancellation, thread killing, priorities or timed waits are needed.
 */
typedef struct quanta_mutex { void *handle; } quanta_mutex;
typedef struct quanta_condition { void *handle; } quanta_condition;
typedef struct quanta_thread { void *handle; } quanta_thread;
typedef void *(*quanta_thread_fn)(void *argument);

int quanta_mutex_init(quanta_mutex *mutex);
void quanta_mutex_destroy(quanta_mutex *mutex);
void quanta_mutex_lock(quanta_mutex *mutex);
void quanta_mutex_unlock(quanta_mutex *mutex);
int quanta_condition_init(quanta_condition *condition);
void quanta_condition_destroy(quanta_condition *condition);
void quanta_condition_wait(quanta_condition *condition, quanta_mutex *mutex);
void quanta_condition_signal(quanta_condition *condition);
void quanta_condition_broadcast(quanta_condition *condition);
int quanta_thread_start(quanta_thread *thread, quanta_thread_fn function, void *argument);
void quanta_thread_join(quanta_thread *thread);

/* Required clock: monotonic, uint64 nanoseconds, callable concurrently, no reset
 * during a session. Resolution may be coarser than 1 ns. Zero means clock failure.
 * sleep may oversleep or return early after host interruption. A wall clock is
 * optional: return 0 when no UTC epoch clock is configured (e.g. ESP8266).
 */
uint64_t quanta_now_ns(void);
uint64_t quanta_realtime_ns(void);
void quanta_sleep_ns(uint64_t nanoseconds);

/* fopen accepts UTF-8 paths on desktop. POSIX implements it directly with fopen;
 * Windows uses _wfopen after UTF-8 conversion. Stream operations above are plain
 * aliases, not a second file abstraction. create_output MUST atomically refuse an
 * existing path; use C11 "wbx" or a native exclusive create, never check-then-open.
 * A port without a filesystem may return NULL/ENOSYS. In-memory queries and
 * serial/stdout output still work, but query-file/output-file require a filesystem.
 */
quanta_file *quanta_fopen(const char *path, const char *mode);
quanta_file *quanta_create_output(const char *path);

/* Optional host hooks. Minimal ports return success, 0 and 0 respectively: no
 * OS signal or process API is required. Core never installs a signal handler.
 * interrupted must be safe to call from worker threads. Zero process ID means
 * unavailable, not an invented OS identity; it is only part of a session tag.
 */
int quanta_install_signal_handlers(void);
int quanta_interrupted(void);
unsigned long quanta_process_id(void);
/* Called once by the app, before it creates threads. Embedders own locale policy.
 * The default implementation uses setlocale(LC_NUMERIC, "C"). A fixed-C-locale
 * runtime can implement this hook as return 1 without providing setlocale. */
int quanta_numeric_locale(void);

/* Portable helpers, implemented once in osal/common.c on the contract above.
 * They are NOT additional OS backend requirements. read_text rejects truncation
 * and embedded NUL; it always terminates an available output buffer.
 */
quanta_status quanta_errno_status(int error);
quanta_status quanta_read_text(const char *path, char *buffer, size_t capacity);
quanta_status quanta_read_u64(const char *path, uint64_t *value);
int quanta_path(char *buffer, size_t capacity, const char *root, const char *relative);
void quanta_utc_string(uint64_t realtime_ns, char *buffer, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
