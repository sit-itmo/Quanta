/* SPDX-License-Identifier: MIT -- small platform-independent window interface.
 */
#ifndef QUANTA_WINDOW_H
#define QUANTA_WINDOW_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
typedef struct quanta_window quanta_window;

enum
{
    QUANTA_X_NONE,
    QUANTA_X_CLOSE,
    QUANTA_X_RESIZE,
    QUANTA_X_MOTION,
    QUANTA_X_BUTTON,
    QUANTA_X_WHEEL,
    QUANTA_X_KEY,
    QUANTA_X_FOCUS
};

enum
{
    QUANTA_X_SHIFT = 1,
    QUANTA_X_CTRL = 2,
    QUANTA_X_ALT = 4,
    QUANTA_X_SUPER = 8
};

typedef struct quanta_window_event
{
    int kind, x, y, button, pressed, width, height, mods;
    uint32_t keysym, codepoint;
} quanta_window_event;

int quanta_window_open(const char *, int, int, quanta_window **, char *, size_t);
int quanta_window_poll(quanta_window *, quanta_window_event *);
int quanta_window_present(quanta_window *, const uint32_t *, int, int);
void quanta_window_close(quanta_window *);
/* Native handle is NULL on XCB and an HWND on Windows. */
void *quanta_window_native(quanta_window *);
float quanta_window_scale(quanta_window *);
#ifdef __cplusplus
}
#endif
#endif
