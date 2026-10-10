/**
 * @file frontends/dispatch.c
 *
 * Dispatch available frontends and return an explicit error for an unavailable mode.
 *
 * Plan: a parsed query containing selected metrics, polling intervals, history, HAL and
 * presentation settings. Parsing does not access hardware.
 *
 * Backend: the windowing/graphics implementation of an interface. The frontend builds widgets
 * without changing how a metric is measured.
 */
/* SPDX-License-Identifier: MIT -- runtime selection stays outside the core */
#include "quanta/view.h"
#include "quanta/osal.h"
#include "quanta_config.h"

const char *quanta_mode_name(quanta_mode mode)
{
    switch (mode)
    {
        case QUANTA_MODE_FILE: return "file";
        case QUANTA_MODE_TUI: return "tui";
        case QUANTA_MODE_GUI: return "gui";
        default: return "unknown";
    }
}
int quanta_mode_available(quanta_mode mode)
{
    switch (mode)
    {
        case QUANTA_MODE_FILE: return 1;
        case QUANTA_MODE_TUI: return QUANTA_WITH_TUI;
        case QUANTA_MODE_GUI: return QUANTA_WITH_GUI;
        default: return 0;
    }
}
#if !QUANTA_WITH_TUI
int quanta_run_tui(quanta_engine *engine, const quanta_plan *plan)
{
    (void)engine; (void)plan;
    quanta_fputs("quanta: TUI was not compiled; use --list-modes or rebuild with TUI=ON\n", quanta_stderr);
    return 2;
}
#endif
#if !QUANTA_WITH_GUI
int quanta_run_gui(quanta_engine *engine, const quanta_plan *plan)
{
    (void)engine; (void)plan;
    quanta_fputs("quanta: GUI was not compiled; use --list-modes or rebuild with GUI=ON\n", quanta_stderr);
    return 2;
}
#endif
int quanta_run_frontend(quanta_engine *engine, const quanta_plan *plan)
{
    if (!engine || !plan) return 2;
    switch (plan->mode)
    {
        case QUANTA_MODE_FILE: return quanta_run_file(engine, plan);
        case QUANTA_MODE_TUI: return quanta_run_tui(engine, plan);
        case QUANTA_MODE_GUI: return quanta_run_gui(engine, plan);
        default: return 2;
    }
}
