/* SPDX-License-Identifier: MIT -- small platform-independent window interface. */
#ifndef INV_WINDOW_H
#define INV_WINDOW_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct inv_window inv_window;
enum {INV_X_NONE,INV_X_CLOSE,INV_X_RESIZE,INV_X_MOTION,INV_X_BUTTON,INV_X_WHEEL,INV_X_KEY,INV_X_FOCUS};
enum {INV_X_SHIFT=1,INV_X_CTRL=2,INV_X_ALT=4,INV_X_SUPER=8};
typedef struct inv_window_event {int kind,x,y,button,pressed,width,height,mods;uint32_t keysym,codepoint;} inv_window_event;
int inv_window_open(const char *,int,int,inv_window **,char *,size_t);
int inv_window_poll(inv_window *,inv_window_event *);
int inv_window_present(inv_window *,const uint32_t *,int,int);
void inv_window_close(inv_window *);
/* Native handle is NULL on XCB and an HWND on Windows. */
void *inv_window_native(inv_window *);
float inv_window_scale(inv_window *);
#ifdef __cplusplus
}
#endif
#endif
