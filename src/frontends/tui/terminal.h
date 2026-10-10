/* SPDX-License-Identifier: MIT */
#ifndef INV_TERMINAL_H
#define INV_TERMINAL_H
#include <stdint.h>
#include <stddef.h>
typedef struct inv_terminal inv_terminal;
enum { INV_KEY_UP=0x110001,INV_KEY_DOWN,INV_KEY_LEFT,INV_KEY_RIGHT,
       INV_KEY_HOME,INV_KEY_END,INV_KEY_PAGE_UP,INV_KEY_PAGE_DOWN };
int inv_terminal_open(inv_terminal **,char *,size_t);
void inv_terminal_close(inv_terminal *);
void inv_terminal_size(inv_terminal *,int *columns,int *rows);
void inv_terminal_begin(inv_terminal *);
void inv_terminal_line(inv_terminal *,const char *utf8,int width);
int inv_terminal_present(inv_terminal *);
/* Returns 1 for one Unicode codepoint/navigation key, 0 for timeout, -1 for error. */
int inv_terminal_key(inv_terminal *,uint32_t *,unsigned timeout_ms);
#endif
