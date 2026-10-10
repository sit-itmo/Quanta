/**
 * @file app/windows_entry.c
 *
 * Windows entry point: convert wide-character arguments to UTF-8 and pass them to the shared
 * CLI.
 *
 * OSAL: the minimal runtime contract for memory, files, clocks, threads and synchronization. OS
 * metrics belong to sensors, not OSAL.
 */
/* SPDX-License-Identifier: MIT -- Unicode command line and UTF-8 byte streams
 */
#include "quanta/osal.h"
#include "windows_internal.h"
#include <fcntl.h>
#include <io.h>
int quanta_cli_main(int, char **);
static UINT input_cp, output_cp;
static int changed_input, changed_output;

static void restore_code_pages(void)
{
    if (changed_input)
    {
        SetConsoleCP(input_cp);
    }
    if (changed_output)
    {
        SetConsoleOutputCP(output_cp);
    }
}

int wmain(int argc, wchar_t **wide_argv)
{
    char **argv = (char **)quanta_calloc((size_t)argc + 1, sizeof(*argv));
    int i, rc = 1;
    DWORD mode;
    if (!argv)
    {
        return 1;
    }
    if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode))
    {
        output_cp = GetConsoleOutputCP();
        changed_output = SetConsoleOutputCP(CP_UTF8) != 0;
    }
    if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode))
    {
        input_cp = GetConsoleCP();
        changed_input = SetConsoleCP(CP_UTF8) != 0;
    }
    atexit(restore_code_pages);
    /* Export files and redirected standard output always contain UTF-8 + LF. */
    (void)_setmode(_fileno(quanta_stdout), _O_BINARY);
    (void)_setmode(_fileno(quanta_stdin), _O_BINARY);
    for (i = 0; i < argc; i++)
    {
        int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_argv[i], -1, NULL, 0,
                                        NULL, NULL);
        if (bytes <= 0)
        {
            goto cleanup;
        }
        argv[i] = (char *)quanta_malloc((size_t)bytes);
        if (!argv[i] || !quanta_win_utf8(argv[i], (size_t)bytes, wide_argv[i]))
        {
            goto cleanup;
        }
    }
    rc = quanta_cli_main(argc, argv);
cleanup:
    for (i = 0; i < argc; i++)
    {
        quanta_free(argv[i]);
    }
    quanta_free(argv);
    return rc;
}
