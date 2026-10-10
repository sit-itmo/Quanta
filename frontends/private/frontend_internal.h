/**
 * @file frontends/private/frontend_internal.h
 *
 * Private declarations for shared text output, formatting and frontend entry points.
 *
 * Sink: a set of engine notification callbacks that decouples data collection from file output,
 * TUI and GUI.
 *
 * Backend: the windowing/graphics implementation of an interface. The frontend builds widgets
 * without changing how a metric is measured.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_FRONTEND_INTERNAL_H
#define QUANTA_FRONTEND_INTERNAL_H
#include "quanta/sensors.h"
#include "quanta/view.h"
#include "support.h"
#endif
