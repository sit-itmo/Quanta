/**
 * @file include/quanta/live.h
 *
 * Public interface for starting a worker and delivering notifications to a consumer through
 * pump.
 *
 * Event: an internal queue record delivered from the worker to a consumer: a series change,
 * sample, frame or diagnostic. This is not an OS event subscription.
 *
 * Sink: a set of engine notification callbacks that decouples data collection from file output,
 * TUI and GUI.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_LIVE_H
#define QUANTA_LIVE_H
#include "engine.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct quanta_live quanta_live;
/* Engine and plan must outlive the worker. Pump on the consumer thread.
 * finish(stop=0) is valid only after done(); otherwise it returns INVALID_DATA
 * without freeing the worker. finish(stop=1) cancels, wakes backpressure and
 * joins. After a pump callback error, the caller must finish(stop=1). */
quanta_status quanta_live_start(quanta_engine *, const quanta_plan *, quanta_live **);
quanta_status quanta_live_pump(quanta_live *, quanta_sink *);
int quanta_live_done(quanta_live *);
quanta_status quanta_live_finish(quanta_live *, int stop);

#ifdef __cplusplus
}
#endif
#endif
