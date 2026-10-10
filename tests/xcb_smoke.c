/* SPDX-License-Identifier: MIT -- standalone real X11 backend smoke test */
#include "raster.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    quanta_window *p = NULL;
    char err[512];
    uint32_t *pixels;
    int x, y;
    if (!quanta_window_open("INV backend smoke test", 640, 400, &p, err, sizeof(err)))
    {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    pixels = (uint32_t *)calloc(640 * 400, sizeof(uint32_t));
    if (!pixels)
    {
        quanta_window_close(p);
        return 1;
    }
    for (y = 0; y < 400; y++)
    {
        for (x = 0; x < 640; x++)
        {
            pixels[y * 640 + x] =
                ((uint32_t)(x * 255 / 639) << 16) | ((uint32_t)(y * 255 / 399) << 8) | 64;
        }
    }
    for (x = 0; x < 20; x++)
    {
        struct timespec t = {0, 50000000};
        quanta_window_event e;
        while (quanta_window_poll(p, &e) > 0)
        {
        }
        if (!quanta_window_present(p, pixels, 640, 400))
        {
            free(pixels);
            quanta_window_close(p);
            return 1;
        }
        nanosleep(&t, NULL);
    }
    free(pixels);
    quanta_window_close(p);
    puts("XCB static backend: window, authentication, events and pixel transport "
         "OK");
    return 0;
}
