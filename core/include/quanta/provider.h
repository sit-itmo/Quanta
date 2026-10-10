/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_PROVIDER_H
#define QUANTA_PROVIDER_H
#include "registry.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct quanta_read_ctx
{
    const quanta_plan *plan;
    const quanta_catalog *catalog;
    quanta_paths paths;
    void *data;
    uint64_t now_ns, acquisition_ns;
    char error[160];
};

typedef struct quanta_discovery_sink
{
    void *context;
    quanta_status (*emit)(void *, const quanta_instance *);
} quanta_discovery_sink;

struct quanta_provider_desc
{
    unsigned index;
    const char *id;
    const char *description;
    quanta_status (*open)(quanta_read_ctx *);
    quanta_status (*discover)(quanta_read_ctx *, quanta_discovery_sink *);
    quanta_status (*refresh)(quanta_read_ctx *);
    void (*close)(quanta_read_ctx *);
};

/* open may leave partially allocated data on failure; close must tolerate
 * NULL/partial data. discover must only emit descriptors from ctx->catalog.
 * refresh acquires one provider batch; read extracts a field from that batch.
 * The scheduler serializes these calls per engine. */
int quanta_metric_wanted(const quanta_read_ctx *context, const quanta_metric_desc *metric);
quanta_status quanta_discover_single(quanta_read_ctx *context, const quanta_provider_desc *provider,
                                     quanta_discovery_sink *sink);

#ifdef __cplusplus
}
#endif
#endif
