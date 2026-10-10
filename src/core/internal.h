/* SPDX-License-Identifier: MIT */
#ifndef INV_INTERNAL_H
#define INV_INTERNAL_H
#include "inv/inv.h"
#include "inv_config.h"
#include <errno.h>
#include <limits.h>
#include "thread.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#define INV_ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define INV_MIN(a,b) ((a)<(b)?(a):(b))
#define INV_MAX(a,b) ((a)>(b)?(a):(b))
#define INV_PROVIDER_COUNT 9
struct inv_read_ctx {
 const inv_plan *plan;
 inv_paths paths;
 void *data;
 uint64_t now_ns, acquisition_ns;
 char error[160];
};
typedef struct inv_discovery_sink {
 void *context;
 inv_status (*emit)(void *, const inv_instance *);
} inv_discovery_sink;
struct inv_provider_desc {
 unsigned index;
 const char *id;
 const char *description;
 inv_status (*open)(inv_read_ctx *);
 inv_status (*discover)(inv_read_ctx *, inv_discovery_sink *);
 inv_status (*refresh)(inv_read_ctx *);
 void (*close)(inv_read_ctx *);
};
extern const inv_provider_desc inv_os_provider, inv_memory_provider,
 inv_cpu_provider, inv_dmi_provider, inv_network_provider, inv_hwmon_provider,
 inv_cpuid_provider, inv_filesystem_provider, inv_process_provider;
inv_status inv_os_read(inv_read_ctx *,inv_instance *,inv_value *,uint64_t *,uint32_t *);
inv_status inv_memory_read(inv_read_ctx *,inv_instance *,inv_value *,uint64_t *,uint32_t *);
inv_status inv_cpu_read(inv_read_ctx *,inv_instance *,inv_value *,uint64_t *,uint32_t *);
inv_status inv_dmi_read(inv_read_ctx *,inv_instance *,inv_value *,uint64_t *,uint32_t *);
inv_status inv_network_read(inv_read_ctx *,inv_instance *,inv_value *,uint64_t *,uint32_t *);
inv_status inv_hwmon_read(inv_read_ctx *,inv_instance *,inv_value *,uint64_t *,uint32_t *);
inv_status inv_cpuid_read(inv_read_ctx *,inv_instance *,inv_value *,uint64_t *,uint32_t *);
inv_status inv_filesystem_read(inv_read_ctx *,inv_instance *,inv_value *,uint64_t *,uint32_t *);
inv_status inv_process_read(inv_read_ctx *,inv_instance *,inv_value *,uint64_t *,uint32_t *);
int inv_metric_wanted(const inv_read_ctx *, const inv_metric_desc *);
inv_status inv_discover_single(inv_read_ctx *,const inv_provider_desc *,inv_discovery_sink *);
inv_status inv_errno_status(int);
FILE *inv_fopen_utf8(const char *, const char *);
FILE *inv_create_output(const char *);
unsigned long inv_process_id(void);
inv_status inv_read_text(const char *, char *, size_t);
inv_status inv_read_u64(const char *, uint64_t *);
int inv_path(char *,size_t,const char *,const char *);
void inv_trim(char *);
int inv_parse_u64(const char *,uint64_t *);
int inv_parse_s64(const char *,int64_t *);
/* Testable normalized parsers; no file I/O. */
typedef struct inv_mem_data { uint64_t total,available,free,swap_total,swap_free;
 unsigned found; } inv_mem_data;
inv_status inv_parse_meminfo(const char *, inv_mem_data *);
inv_status inv_cpu_fraction(const uint64_t prev[8],const uint64_t now[8],double *);
inv_status inv_counter_derive(inv_transform,const inv_sample *,const inv_sample *,inv_value *,uint64_t *);
#endif
