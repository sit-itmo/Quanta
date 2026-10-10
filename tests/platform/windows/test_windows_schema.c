/* SPDX-License-Identifier: MIT -- validates the REAL Windows
 * descriptors/queries, NOT the native acquisition implementation. No acquisition
 * is attempted.
 */
#include "quanta/osal.h"
#include "collector_internal.h"

int main(int argc, char **argv)
{
    size_t n, i;
    const quanta_metric_desc *const *metrics = quanta_registry(&n);
    quanta_plan *p = (quanta_plan *)quanta_malloc(sizeof(*p));
    char error[512], query[65537];
    if (!p)
    {
        return 1;
    }
    if (quanta_registry_validate(error, sizeof(error)) != QUANTA_OK)
    {
        quanta_fprintf(quanta_stderr, "%s\n", error);
        quanta_free(p);
        return 1;
    }
    for (i = 0; i < n; i++)
    {
#if QUANTA_EMBED_DOCS
        if (!quanta_metric_documentation(metrics[i]->key))
        {
            quanta_fprintf(quanta_stderr, "Missing doc: %s\n", metrics[i]->key);
            quanta_free(p);
            return 1;
        }
#else
        (void)metrics;
#endif
    }
    for (i = 1; i < (size_t)argc; i++)
    {
        size_t bytes;
        quanta_file *f = fopen(argv[i], "rb");
        if (!f)
        {
            perror(argv[i]);
            quanta_free(p);
            return 1;
        }
        bytes = quanta_fread(query, 1, sizeof(query) - 1, f);
        if (quanta_ferror(f) || !quanta_feof(f))
        {
            quanta_fclose(f);
            quanta_free(p);
            return 1;
        }
        quanta_fclose(f);
        query[bytes] = 0;
        if (quanta_query_parse(query, p, error, sizeof(error)) != QUANTA_OK ||
            quanta_plan_validate(p, error, sizeof(error)) != QUANTA_OK)
        {
            quanta_fprintf(quanta_stderr, "%s: %s\n", argv[i], error);
            quanta_free(p);
            return 1;
        }
    }
    quanta_free(p);
    quanta_printf("Windows schema only: %zu metric descriptors; %d query files "
           "validated (no WinAPI "
           "execution)\n",
           n, argc - 1);
    return 0;
}
