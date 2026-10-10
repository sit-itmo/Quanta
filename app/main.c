/**
 * @file app/main.c
 *
 * Entry point of the single quanta utility: arguments, query loading, mode selection and engine
 * startup.
 *
 * Plan: a parsed query containing selected metrics, polling intervals, history, HAL and
 * presentation settings. Parsing does not access hardware.
 *
 * Catalog: an immutable set of metric descriptors with unique names. It neither stores
 * measurement results nor selects providers.
 *
 * Sink: a set of engine notification callbacks that decouples data collection from file output,
 * TUI and GUI.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "frontend_internal.h"
#include "quanta/view.h"
#include "quanta/hal.h"
#include "quanta_config.h"

static void usage(quanta_file *f)
{
    quanta_fputs("quanta " QUANTA_VERSION " -- portable inventory and monitoring\n\n"
          "  quanta list [GLOB] [--compiled | --probe]\n"
          "  quanta describe KEY\n"
          "  quanta get SELECTOR... [options]\n"
          "  quanta run --query 'Quanta Query...' [options]\n"
          "  quanta run --query-file dashboard.q [options]\n"
          "  quanta explain --query-file dashboard.q\n\n"
          "  quanta hal-info [--hal NAME]\n"
          "  quanta --list-hals\n\n"
          "Options:\n"
          "  --hal NAME           Override the query HAL (default native)\n"
          "  -q, --query TEXT      Inline Quanta Query query\n"
          "  --query-file PATH     Read Quanta Query, '-' reads standard input\n"
          "  --mode cli|file|tui|gui  Override the query frontend\n"
          "  --cli | --tui | --gui   Select the frontend (short form)\n"
          "  --list-modes          List compiled frontends\n"
          "  --format human|table|txt|csv|md|value\n"
          "  --every DURATION     Global requested period (e.g. 250ms, 1s)\n"
          "  --duration DURATION  Stop after this elapsed time\n"
          "  --count N            Stop after N output frames (including warmup)\n"
          "  --once               Single snapshot\n"
          "  --warmup DURATION    Prime rate/CPU baselines before a single "
          "snapshot\n"
          "  --output PATH        Create a NEW output file, '-' is stdout\n"
          "  --redact             Mask sensitive values in the active frontend\n"
          "  --strict             Reject periods below metric minimums\n"
          "  --fail-on-error      Exit 3 on measurement errors (not WARMUP)\n"
          "  --csv-unsafe-strings Disable spreadsheet formula neutralization\n"
          "  --font PATH          GUI: load a local TTF font (e.g. for Cyrillic)\n"
          "  --version, --help\n\n"
          "TUI keys: q quit, p pause display, r refresh properties/discovery,\n"
          "          / filter, Esc clear filter, j/k or arrows scroll, s sort.\n"
          "All collectors are read-only. No elevation, external commands or "
          "telemetry.\n",
          f);
}

static int append(char *s, size_t cap, const char *v)
{
    size_t n = quanta_strlen(s), m = quanta_strlen(v);
    if (m >= cap - n)
    {
        return 0;
    }
    quanta_memcpy(s + n, v, m + 1);
    return 1;
}

static int load_query(const char *path, char *s, size_t cap)
{
    quanta_file *f;
    size_t n;
    int extra;
    f = !quanta_strcmp(path, "-") ? quanta_stdin : quanta_fopen(path, "rb");
    if (!f)
    {
        quanta_fprintf(quanta_stderr, "quanta: query file: %s\n", quanta_strerror(quanta_errno));
        return 0;
    }
    n = quanta_fread(s, 1, cap - 1, f);
    extra = quanta_fgetc(f);
    if (quanta_ferror(f) || extra != QUANTA_EOF)
    {
        if (f != quanta_stdin)
        {
            quanta_fclose(f);
        }
        quanta_fprintf(quanta_stderr, "quanta: query read failed or query too large\n");
        return 0;
    }
    if (quanta_memchr(s, 0, n))
    {
        if (f != quanta_stdin)
        {
            quanta_fclose(f);
        }
        quanta_fprintf(quanta_stderr, "quanta: NUL is not permitted in a query\n");
        return 0;
    }
    s[n] = 0;
    /* PowerShell UTF-8 files may contain a BOM. UTF-16 remains rejected. */
    if (n >= 3 && (unsigned char)s[0] == 0xef && (unsigned char)s[1] == 0xbb &&
        (unsigned char)s[2] == 0xbf)
    {
        quanta_memmove(s, s + 3, n - 2);
    }
    if (f != quanta_stdin)
    {
        quanta_fclose(f);
    }
    return 1;
}

static int parse_mode(const char *s, quanta_mode *m)
{
    if (!quanta_strcmp(s, "gui"))
    {
        *m = QUANTA_MODE_GUI;
    }
    else if (!quanta_strcmp(s, "tui"))
    {
        *m = QUANTA_MODE_TUI;
    }
    else if ((!quanta_strcmp(s, "file") || !quanta_strcmp(s, "cli")))
    {
        *m = QUANTA_MODE_FILE;
    }
    else
    {
        return 0;
    }
    return 1;
}

static int parse_format(const char *s, quanta_format *f)
{
    if (!quanta_strcmp(s, "human"))
    {
        *f = QUANTA_FORMAT_HUMAN;
    }
    else if ((!quanta_strcmp(s, "table") || !quanta_strcmp(s, "txt")))
    {
        *f = QUANTA_FORMAT_TABLE;
    }
    else if (!quanta_strcmp(s, "csv"))
    {
        *f = QUANTA_FORMAT_CSV;
    }
    else if (!quanta_strcmp(s, "md"))
    {
        *f = QUANTA_FORMAT_MD;
    }
    else if (!quanta_strcmp(s, "value"))
    {
        *f = QUANTA_FORMAT_VALUE;
    }
    else
    {
        return 0;
    }
    return 1;
}

static void list_compiled(const char *pattern)
{
    size_t i, n;
    const quanta_metric_desc *const *r = quanta_registry(&n);
    quanta_printf("registry count = %zu\n", n);
    quanta_printf("%-36s %-7s %-9s %-9s %-8s %s\n", "KEY", "TYPE", "KIND", "UNIT", "MIN(s)",
           "DESCRIPTION");
    for (i = 0; i < n; i++)
    {
        if (quanta_glob_match(pattern, r[i]->key))
        {
            quanta_printf("%-36s %-7s %-9s %-9s %-8.3f %s\n", r[i]->key, quanta_type_name(r[i]->type),
                   quanta_kind_name(r[i]->kind), r[i]->unit, (double)r[i]->min_poll_ns / 1e9,
                   r[i]->description);
        }
    }
}

static int describe(const char *key)
{
    const quanta_metric_desc *m = quanta_metric_find(key);
    const char *doc;
    if (!m)
    {
        quanta_fprintf(quanta_stderr, "quanta: metric is not compiled: %s\n", key);
        return 2;
    }
    quanta_printf("%s -- %s\n\n%s\n\nType: %s\nKind: %s\nUnit: %s\nSemantic version: "
           "%u\nAcquisition: "
           "%s\nLabels: %s\nMinimum poll interval: %.3fs\nSensitive: "
           "%s\nDocumentation: %s\n",
           m->key, m->title, m->description, quanta_type_name(m->type), quanta_kind_name(m->kind),
           m->unit, m->semantic_version, quanta_metric_source(m), m->label_schema,
           (double)m->min_poll_ns / 1e9, m->flags & QUANTA_SENSITIVE ? "yes" : "no", m->doc_path);
    doc = quanta_metric_documentation(key);
    if (doc)
    {
        quanta_printf("\n---\n%s\n", doc);
    }
    return 0;
}

int quanta_cli_main(int argc, char **argv)
{
    quanta_plan *plan;
    quanta_engine *engine = NULL;
    char *query;
    char err[512];
    const char *command = "run", *pattern = "*", *key = NULL, *query_arg = NULL, *query_file = NULL,
               *output = NULL, *font = NULL, *hal_name = NULL;
    int i = 1, mode_set = 0, format_set = 0, probe = 0, get_count = 0, once = 0, redact = 0,
        strict = 0, fail_error = 0, csv_safe = 1, rc = 0;
    quanta_mode mode = QUANTA_MODE_FILE;
    quanta_format format = QUANTA_FORMAT_HUMAN;
    uint64_t period = 0, duration_ns = 0, count = 0, warmup = 0;
    quanta_status st;
    if (!quanta_numeric_locale())
    {
        quanta_fputs("quanta: cannot select the C numeric locale\n", quanta_stderr);
        return 1;
    }
    if (!quanta_install_signal_handlers())
    {
        quanta_fputs("quanta: cannot install signal handlers\n", quanta_stderr);
        return 1;
    }
    if (argc == 1)
    {
        usage(quanta_stdout);
        return 0;
    }
    if (!quanta_strcmp(argv[1], "--list-modes"))
    {
        if (argc != 2) return 2;
        for (int mode_index = QUANTA_MODE_FILE; mode_index <= QUANTA_MODE_GUI; ++mode_index)
            if (quanta_mode_available((quanta_mode)mode_index))
                quanta_puts(quanta_mode_name((quanta_mode)mode_index));
        return 0;
    }
    if (!quanta_strcmp(argv[1], "--list-hals"))
    {
        size_t h, n;
        const char *const *names = quanta_hal_names(&n);
        for (h = 0; h < n; ++h) quanta_puts(names[h]);
        return 0;
    }
    if (!quanta_strcmp(argv[1], "--version"))
    {
        size_t n;
        quanta_registry(&n);
        quanta_printf("quanta %s | C11 | metrics=%zu | TUI=%d GUI=%d static=%d | ImGui "
               "adapter=1.92.9b\n",
               QUANTA_VERSION, n, QUANTA_WITH_TUI, QUANTA_WITH_GUI, QUANTA_STRICT_STATIC);
        return 0;
    }
    if (!quanta_strcmp(argv[1], "--help") || !quanta_strcmp(argv[1], "-h"))
    {
        usage(quanta_stdout);
        return 0;
    }
    if (argv[1][0] != '-')
    {
        command = argv[1];
        i++;
    }
    if (quanta_strcmp(command, "run") && quanta_strcmp(command, "get") && quanta_strcmp(command, "explain") &&
        quanta_strcmp(command, "list") && quanta_strcmp(command, "describe") && quanta_strcmp(command, "hal-info"))
    {
        size_t length = quanta_strlen(command);
        if (length > 2 && !quanta_strcmp(command + length - 2, ".q"))
        {
            query_file = command;
            command = "run";
        }
        else
        {
            quanta_fprintf(quanta_stderr, "quanta: unknown command '%s'\n", command);
            return 2;
        }
    }
    query = (char *)quanta_calloc(QUANTA_QUERY_CAP + 1, 1);
    plan = (quanta_plan *)quanta_malloc(sizeof(*plan));
    if (!query || !plan)
    {
        quanta_free(query);
        quanta_free(plan);
        return 1;
    }
    for (; i < argc; i++)
    {
        const char *a = argv[i], *v = NULL;
        if (!quanta_strcmp(a, "--help"))
        {
            usage(quanta_stdout);
            goto done;
        }
        if (!quanta_strcmp(a, "--gui") || !quanta_strcmp(a, "--tui") || !quanta_strcmp(a, "--cli"))
        {
            if (mode_set)
            {
                quanta_snprintf(err, sizeof(err), "frontend mode specified more than once");
                goto bad;
            }
            parse_mode(a + 2, &mode);
            mode_set = 1;
            continue;
        }
        if (!quanta_strcmp(a, "--probe"))
        {
            probe = 1;
            continue;
        }
        if (!quanta_strcmp(a, "--compiled"))
        {
            continue;
        }
        if (!quanta_strcmp(a, "--once"))
        {
            once = 1;
            continue;
        }
        if (!quanta_strcmp(a, "--redact"))
        {
            redact = 1;
            continue;
        }
        if (!quanta_strcmp(a, "--strict"))
        {
            strict = 1;
            continue;
        }
        if (!quanta_strcmp(a, "--fail-on-error"))
        {
            fail_error = 1;
            continue;
        }
        if (!quanta_strcmp(a, "--csv-unsafe-strings"))
        {
            csv_safe = 0;
            continue;
        }
        if (a[0] != '-')
        {
            if (!quanta_strcmp(command, "list"))
            {
                if (quanta_strcmp(pattern, "*"))
                {
                    quanta_snprintf(err, sizeof(err), "only one list pattern is allowed");
                    goto bad;
                }
                pattern = a;
            }
            else if (!quanta_strcmp(command, "describe"))
            {
                if (key)
                {
                    quanta_snprintf(err, sizeof(err), "describe takes exactly one metric key");
                    goto bad;
                }
                key = a;
            }
            else if (!quanta_strcmp(command, "get"))
            {
                if (!append(query, QUANTA_QUERY_CAP + 1, "select ") ||
                    !append(query, QUANTA_QUERY_CAP + 1, a) ||
                    !append(query, QUANTA_QUERY_CAP + 1, ";\n"))
                {
                    quanta_snprintf(err, sizeof(err), "selectors too long");
                    goto bad;
                }
                get_count++;
            }
            else
            {
                quanta_snprintf(err, sizeof(err), "unexpected argument '%s'", a);
                goto bad;
            }
            continue;
        }
        if (i + 1 >= argc)
        {
            quanta_snprintf(err, sizeof(err), "option %s requires a value", a);
            goto bad;
        }
        v = argv[++i];
        if (!quanta_strcmp(a, "--query") || !quanta_strcmp(a, "-q"))
        {
            if (query_arg || query_file)
            {
                quanta_snprintf(err, sizeof(err), "use one query source");
                goto bad;
            }
            query_arg = v;
        }
        else if (!quanta_strcmp(a, "--query-file"))
        {
            if (query_arg || query_file)
            {
                quanta_snprintf(err, sizeof(err), "use one query source");
                goto bad;
            }
            query_file = v;
        }
        else if (!quanta_strcmp(a, "--hal"))
        {
            if (hal_name || !v[0])
            {
                quanta_snprintf(err, sizeof(err), "--hal requires one nonempty name");
                goto bad;
            }
            hal_name = v;
        }
        else if (!quanta_strcmp(a, "--mode"))
        {
            if (mode_set || !parse_mode(v, &mode))
            {
                quanta_snprintf(err, sizeof(err), "invalid mode: %s", v);
                goto bad;
            }
            mode_set = 1;
        }
        else if (!quanta_strcmp(a, "--format"))
        {
            if (!parse_format(v, &format))
            {
                quanta_snprintf(err, sizeof(err), "invalid format: %s", v);
                goto bad;
            }
            format_set = 1;
        }
        else if (!quanta_strcmp(a, "--output"))
        {
            output = v;
        }
        else if (!quanta_strcmp(a, "--font"))
        {
            font = v;
        }
        else if (!quanta_strcmp(a, "--every"))
        {
            if (quanta_parse_duration(v, &period) != QUANTA_OK)
            {
                quanta_snprintf(err, sizeof(err), "invalid period: %s", v);
                goto bad;
            }
        }
        else if (!quanta_strcmp(a, "--duration"))
        {
            if (quanta_parse_duration(v, &duration_ns) != QUANTA_OK)
            {
                quanta_snprintf(err, sizeof(err), "invalid duration: %s", v);
                goto bad;
            }
        }
        else if (!quanta_strcmp(a, "--warmup"))
        {
            if (quanta_parse_duration(v, &warmup) != QUANTA_OK)
            {
                quanta_snprintf(err, sizeof(err), "invalid warmup: %s", v);
                goto bad;
            }
        }
        else if (!quanta_strcmp(a, "--count"))
        {
            if (!quanta_parse_u64(v, &count) || !count)
            {
                quanta_snprintf(err, sizeof(err), "invalid count: %s", v);
                goto bad;
            }
        }
        else
        {
            quanta_snprintf(err, sizeof(err), "unknown option: %s", a);
            goto bad;
        }
    }
    if (!quanta_strcmp(command, "hal-info"))
    {
        quanta_hal *hal = NULL;
        quanta_hal_info info;
        quanta_hal_options options = {0};
        options.name = hal_name;
        st = quanta_hal_open(&options, &hal);
        if (st != QUANTA_OK)
        {
            quanta_snprintf(err, sizeof(err), "cannot open HAL '%s': %s", hal_name ? hal_name : "native", quanta_status_name(st));
            goto bad;
        }
        quanta_hal_get_info(hal, &info);
        quanta_printf("HAL: %s\nImplemented operations: 0x%08x\nRegister kinds: 0x%08x\n"
               "Hardware writes: %s\nLogical CPUs: %u (%s)\n"
               "OS-reported physical memory: %" PRIu64 " bytes (%s)\n"
               "Maximum transfer: %u bytes\n"
               "Capabilities describe implementation, not permission or safe address ranges.\n",
               info.name, info.capabilities, info.register_kinds,
               info.writes_enabled ? "enabled" : "disabled",
               info.logical_cpus, info.known & QUANTA_HAL_CPU_COUNT_KNOWN ? "known" : "unknown",
               info.physical_memory_bytes, info.known & QUANTA_HAL_MEMORY_SIZE_KNOWN ? "known" : "unknown",
               info.maximum_transfer);
        quanta_hal_close(hal);
        goto done;
    }
    st = quanta_registry_validate(err, sizeof(err));
    if (st != QUANTA_OK)
    {
        goto bad;
    }
    if (!quanta_strcmp(command, "describe"))
    {
        if (!key)
        {
            quanta_snprintf(err, sizeof(err), "describe requires a key");
            goto bad;
        }
        rc = describe(key);
        goto done;
    }
    if (!quanta_strcmp(command, "list") && !probe)
    {
        list_compiled(pattern);
        goto done;
    }
    if (get_count && (query_arg || query_file))
    {
        quanta_snprintf(err, sizeof(err), "get selectors cannot be combined with a query");
        goto bad;
    }
    if (query_arg)
    {
        if (!quanta_copy(query, QUANTA_QUERY_CAP + 1, query_arg))
        {
            quanta_snprintf(err, sizeof(err), "query too long");
            goto bad;
        }
    }
    else if (query_file)
    {
        if (!load_query(query_file, query, QUANTA_QUERY_CAP + 1))
        {
            rc = 2;
            goto done;
        }
    }
    else if (!quanta_strcmp(command, "list") && probe)
    {
        if (quanta_snprintf(query, QUANTA_QUERY_CAP + 1,
                     "mode file; format table; once; select optional %s;",
                     pattern) > QUANTA_QUERY_CAP)
        {
            quanta_snprintf(err, sizeof(err), "pattern too long");
            goto bad;
        }
    }
    else if (!query[0])
    {
        if (!quanta_strcmp(command, "get"))
        {
            quanta_snprintf(err, sizeof(err), "get requires at least one selector");
            goto bad;
        }
        quanta_copy(query, QUANTA_QUERY_CAP + 1,
                    "select quanta.version; select optional os.name; select optional os.memory.utilization as "
                    "bar(min=0,max=1); select "
                    "optional cpu.utilization as plot(min=0,max=1); select optional "
                    "sensor.temperature as "
                    "value; select optional rate(device.net.rx_bytes_total) as plot; ");
    }
    st = quanta_query_parse(query, plan, err, sizeof(err));
    if (st != QUANTA_OK)
    {
        goto bad;
    }
    if (hal_name && !quanta_copy(plan->hal, sizeof(plan->hal), hal_name))
    {
        quanta_snprintf(err, sizeof(err), "HAL name is too long");
        goto bad;
    }
    if (mode_set)
    {
        plan->mode = mode;
    }
    if (format_set)
    {
        plan->format = format;
    }
    if (period)
    {
        size_t j;
        for (j = 0; j < plan->select_count; j++)
        {
            if (!plan->selects[j].period_ns)
            {
                plan->selects[j].period_ns = period;
            }
        }
        plan->period_ns = period;
        plan->once = 0;
    }
    if (duration_ns)
    {
        plan->duration_ns = duration_ns;
        plan->once = 0;
    }
    if (count)
    {
        plan->count = count;
        plan->once = 0;
    }
    if (once)
    {
        plan->once = 1;
        plan->once_explicit = 1;
    }
    if (output && !quanta_copy(plan->output, sizeof(plan->output), output))
    {
        quanta_snprintf(err, sizeof(err), "output path too long");
        goto bad;
    }
    if (font && !quanta_copy(plan->font_path, sizeof(plan->font_path), font))
    {
        quanta_snprintf(err, sizeof(err), "font path too long");
        goto bad;
    }
    plan->redact = redact;
    plan->strict_poll |= strict;
    plan->fail_on_error = fail_error;
    plan->csv_safe = csv_safe;
    plan->warmup_ns = warmup;
    if (warmup && !plan->once)
    {
        quanta_snprintf(err, sizeof(err), "--warmup requires a one-shot file report");
        goto bad;
    }
    if (!quanta_mode_available(plan->mode))
    {
        quanta_snprintf(err, sizeof(err), "%s frontend was not compiled; use --list-modes",
                        quanta_mode_name(plan->mode));
        goto bad;
    }
    st = quanta_plan_validate(plan, err, sizeof(err));
    if (st != QUANTA_OK)
    {
        goto bad;
    }
    st = quanta_engine_create(plan, NULL, &engine, err, sizeof(err));
    if (st != QUANTA_OK)
    {
        quanta_fprintf(quanta_stderr, "quanta: %s\n", err);
        rc = st == QUANTA_NOT_PRESENT ? 3 : 1;
        goto done;
    }
    if (!quanta_strcmp(command, "explain"))
    {
        quanta_engine_explain(engine, quanta_stdout);
        goto done;
    }
    rc = quanta_run_frontend(engine, plan);
    if (!rc && plan->fail_on_error && quanta_engine_has_sample_errors(engine))
    {
        rc = 3;
    }
    if (quanta_interrupted())
    {
        rc = 128 + quanta_interrupted();
    }
    goto done;
bad:
    quanta_fprintf(quanta_stderr, "quanta: %s\n", err);
    rc = 2;
done:
    quanta_engine_destroy(engine);
    quanta_free(query);
    quanta_free(plan);
    return rc;
}

#ifndef _WIN32
int main(int argc, char **argv)
{
    return quanta_cli_main(argc, argv);
}
#endif
