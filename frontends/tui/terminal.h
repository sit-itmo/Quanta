/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_TERMINAL_H
#define QUANTA_TERMINAL_H
#include <stddef.h>
#include <stdint.h>
typedef struct quanta_terminal quanta_terminal;

enum
{
    QUANTA_KEY_UP = 0x110001,
    QUANTA_KEY_DOWN,
    QUANTA_KEY_LEFT,
    QUANTA_KEY_RIGHT,
    QUANTA_KEY_HOME,
    QUANTA_KEY_END,
    QUANTA_KEY_PAGE_UP,
    QUANTA_KEY_PAGE_DOWN
};

int quanta_terminal_open(quanta_terminal **, char *, size_t);
void quanta_terminal_close(quanta_terminal *);
void quanta_terminal_size(quanta_terminal *, int *columns, int *rows);
void quanta_terminal_begin(quanta_terminal *);
void quanta_terminal_line(quanta_terminal *, const char *utf8, int width);
int quanta_terminal_present(quanta_terminal *);
/* Returns 1 for one Unicode codepoint/navigation key, 0 for timeout, -1 for
 * error. */
int quanta_terminal_key(quanta_terminal *, uint32_t *, unsigned timeout_ms);
#endif
