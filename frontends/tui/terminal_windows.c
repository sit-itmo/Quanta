/* SPDX-License-Identifier: MIT -- native screen buffer; no VT sequences or
 * curses */
#include "quanta/osal.h"
#include "terminal.h"
#include "windows_internal.h"

struct quanta_terminal
{
    HANDLE input, original, screen;
    DWORD saved_mode;
    CHAR_INFO *cells;
    size_t capacity;
    int cols, rows, row, failed;
    SMALL_RECT viewport;
    uint32_t repeated;
    unsigned repeats;
    wchar_t high_surrogate;
};

int quanta_terminal_open(quanta_terminal **out, char *error, size_t cap)
{
    quanta_terminal *t = (quanta_terminal *)quanta_calloc(1, sizeof(*t));
    DWORD mode, output_mode;
    CONSOLE_CURSOR_INFO cursor = {1, FALSE};
    *out = NULL;
    if (!t)
    {
        quanta_snprintf(error, cap, "TUI: out of memory");
        return 0;
    }
    t->input = GetStdHandle(STD_INPUT_HANDLE);
    t->original = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(t->input, &mode) || !GetConsoleMode(t->original, &output_mode))
    {
        quanta_snprintf(error, cap,
                 "TUI needs a Windows console (PowerShell, cmd or Windows "
                 "Terminal), not "
                 "redirected handles");
        quanta_free(t);
        return 0;
    }
    t->saved_mode = mode;
    t->screen =
        CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (t->screen == INVALID_HANDLE_VALUE)
    {
        quanta_snprintf(error, cap, "CreateConsoleScreenBuffer: error %lu", (unsigned long)GetLastError());
        quanta_free(t);
        return 0;
    }
    mode = (mode | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS) &
           ~(DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_QUICK_EDIT_MODE);
    if (!SetConsoleMode(t->input, mode) || !SetConsoleActiveScreenBuffer(t->screen))
    {
        DWORD e = GetLastError();
        (void)SetConsoleMode(t->input, t->saved_mode);
        CloseHandle(t->screen);
        quanta_free(t);
        quanta_snprintf(error, cap, "TUI console initialization: error %lu", (unsigned long)e);
        return 0;
    }
    (void)SetConsoleCursorInfo(t->screen, &cursor);
    *out = t;
    return 1;
}

void quanta_terminal_close(quanta_terminal *t)
{
    if (!t)
    {
        return;
    }
    (void)SetConsoleActiveScreenBuffer(t->original);
    (void)SetConsoleMode(t->input, t->saved_mode);
    CloseHandle(t->screen);
    quanta_free(t->cells);
    quanta_free(t);
}

void quanta_terminal_size(quanta_terminal *t, int *cols, int *rows)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(t->screen, &info))
    {
        t->failed = 1;
        *cols = 80;
        *rows = 25;
        return;
    }
    t->viewport = info.srWindow;
    *cols = (int)(info.srWindow.Right - info.srWindow.Left + 1);
    *rows = (int)(info.srWindow.Bottom - info.srWindow.Top + 1);
    t->cols = QUANTA_MIN(*cols, 500);
    t->rows = QUANTA_MIN(*rows, 300);
    *cols = t->cols;
    *rows = t->rows;
}

void quanta_terminal_begin(quanta_terminal *t)
{
    size_t cells = (size_t)t->cols * (size_t)t->rows, i;
    t->row = 0;
    if (!cells || cells > 150000)
    {
        t->failed = 1;
        return;
    }
    if (cells > t->capacity)
    {
        CHAR_INFO *data = (CHAR_INFO *)quanta_realloc(t->cells, cells * sizeof(*data));
        if (!data)
        {
            t->failed = 1;
            return;
        }
        t->cells = data;
        t->capacity = cells;
    }
    for (i = 0; i < cells; i++)
    {
        t->cells[i].Char.UnicodeChar = L' ';
        t->cells[i].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }
}

void quanta_terminal_line(quanta_terminal *t, const char *utf8, int width)
{
    wchar_t text[4096];
    char safe[4096];
    int length, n, i;
    WORD color = t->row == 0 ? (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
                             : (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    if (t->failed || t->row >= t->rows)
    {
        return;
    }
    quanta_safe_text(safe, sizeof(safe), utf8);
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, safe, -1, text,
                                 (int)QUANTA_ARRAY_SIZE(text));
    if (!length)
    {
        t->row++;
        return;
    }
    n = QUANTA_MIN(length - 1, QUANTA_MIN(width - 1, t->cols));
    /* Do not split a UTF-16 surrogate pair at the right edge. */
    if (n > 0 && text[n - 1] >= 0xd800 && text[n - 1] <= 0xdbff)
    {
        n--;
    }
    for (i = 0; i < n; i++)
    {
        CHAR_INFO *cell = &t->cells[(size_t)t->row * (size_t)t->cols + (size_t)i];
        cell->Char.UnicodeChar = text[i];
        cell->Attributes = color;
    }
    t->row++;
}

int quanta_terminal_present(quanta_terminal *t)
{
    COORD size, origin = {0, 0};
    SMALL_RECT rectangle = t->viewport;
    if (t->failed)
    {
        return 0;
    }
    size.X = (SHORT)t->cols;
    size.Y = (SHORT)t->rows;
    rectangle.Right = (SHORT)(rectangle.Left + t->cols - 1);
    rectangle.Bottom = (SHORT)(rectangle.Top + t->rows - 1);
    return WriteConsoleOutputW(t->screen, t->cells, size, origin, &rectangle) != 0;
}

int quanta_terminal_key(quanta_terminal *t, uint32_t *key, unsigned timeout)
{
    INPUT_RECORD event;
    DWORD read = 0, result;
    KEY_EVENT_RECORD *k;
    wchar_t ch;
    if (t->repeats)
    {
        t->repeats--;
        *key = t->repeated;
        return 1;
    }
    result = WaitForSingleObject(t->input, (DWORD)timeout);
    if (result == WAIT_TIMEOUT)
    {
        return 0;
    }
    if (result != WAIT_OBJECT_0)
    {
        return -1;
    }
    if (!ReadConsoleInputW(t->input, &event, 1, &read))
    {
        return -1;
    }
    if (!read || event.EventType != KEY_EVENT || !event.Event.KeyEvent.bKeyDown)
    {
        return 0;
    }
    k = &event.Event.KeyEvent;
    ch = k->uChar.UnicodeChar;
    switch (k->wVirtualKeyCode)
    {
        case VK_UP:
            *key = QUANTA_KEY_UP;
            break;
        case VK_DOWN:
            *key = QUANTA_KEY_DOWN;
            break;
        case VK_LEFT:
            *key = QUANTA_KEY_LEFT;
            break;
        case VK_RIGHT:
            *key = QUANTA_KEY_RIGHT;
            break;
        case VK_HOME:
            *key = QUANTA_KEY_HOME;
            break;
        case VK_END:
            *key = QUANTA_KEY_END;
            break;
        case VK_PRIOR:
            *key = QUANTA_KEY_PAGE_UP;
            break;
        case VK_NEXT:
            *key = QUANTA_KEY_PAGE_DOWN;
            break;
        case VK_ESCAPE:
            *key = 27;
            break;
        default:
            if (ch >= 0xd800 && ch <= 0xdbff)
            {
                t->high_surrogate = ch;
                return 0;
            }
            if (ch >= 0xdc00 && ch <= 0xdfff)
            {
                if (!t->high_surrogate)
                {
                    return 0;
                }
                *key = 0x10000u + (((uint32_t)t->high_surrogate - 0xd800u) << 10) + (uint32_t)ch -
                       0xdc00u;
            }
            else
            {
                if (!ch)
                {
                    return 0;
                }
                *key = ch;
            }
            t->high_surrogate = 0;
            break;
    }
    t->repeated = *key;
    t->repeats = k->wRepeatCount ? (unsigned)k->wRepeatCount - 1 : 0;
    return 1;
}
