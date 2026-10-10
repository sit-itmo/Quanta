/* SPDX-License-Identifier: MIT -- platform-neutral Windows data tests.
 * These tests exercise our parser/math, NOT execution of Windows APIs.
 */
#include "collector_internal.h"
#include "frontend_internal.h"
#include "smbios.h"
#include "windows_math.h"
#include <math.h>
static unsigned checks, failures;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        checks++;                                                                                  \
        if (!(x))                                                                                  \
        {                                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

typedef struct builder
{
    uint8_t bytes[8192];
    size_t size;
} builder;

static void put16(uint8_t *p, uint16_t n)
{
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
}

static void put32(uint8_t *p, uint32_t n)
{
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
    p[2] = (uint8_t)(n >> 16);
    p[3] = (uint8_t)(n >> 24);
}

static uint8_t *record(builder *b, unsigned type, unsigned len, unsigned handle)
{
    uint8_t *p = b->bytes + b->size;
    CHECK(b->size + len + 4 < sizeof(b->bytes));
    memset(p, 0, len);
    p[0] = (uint8_t)type;
    p[1] = (uint8_t)len;
    put16(p + 2, (uint16_t)handle);
    b->size += len;
    return p;
}

static void strings(builder *b, const char **text, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++)
    {
        size_t n = strlen(text[i]) + 1;
        CHECK(b->size + n + 2 < sizeof(b->bytes));
        memcpy(b->bytes + b->size, text[i], n);
        b->size += n;
    }
    if (!count)
    {
        b->bytes[b->size++] = 0;
    }
    b->bytes[b->size++] = 0;
}

static void end_table(builder *b)
{
    record(b, 127, 4, 0xffff);
    strings(b, NULL, 0);
}

static void test_math(void)
{
    uint64_t n = 0;
    double f = 0;
    unsigned g, c;
    CHECK(quanta_win_memory_used(1000, 400, &n, &f) == QUANTA_OK && n == 600 &&
          fabs(f - .6) < 1e-12);
    CHECK(quanta_win_memory_used(0, 0, &n, &f) == QUANTA_INVALID_DATA);
    CHECK(quanta_win_memory_used(100, 101, &n, &f) == QUANTA_INVALID_DATA);
    CHECK(quanta_win_memory_used(UINT64_MAX, 0, &n, &f) == QUANTA_OK && n == UINT64_MAX && f == 1);
    CHECK(quanta_win_cpu_fraction(100, 50, 200, 70, &f) == QUANTA_OK && fabs(f - .8) < 1e-12);
    CHECK(quanta_win_cpu_fraction(100, 50, 100, 50, &f) == QUANTA_WARMUP);
    CHECK(quanta_win_cpu_fraction(100, 50, 200, 40, &f) == QUANTA_WARMUP);
    CHECK(quanta_win_cpu_fraction(100, 50, 200, 180, &f) == QUANTA_WARMUP);
    CHECK(quanta_win_cpu_fraction(100, 101, 200, 190, &f) == QUANTA_INVALID_DATA);
    CHECK(quanta_win_cpu_fraction(UINT64_MAX - 10, 0, UINT64_MAX, 5, &f) == QUANTA_OK && f == .5);
    CHECK(quanta_win_pages_bytes(UINT64_MAX / 4096, 4096, &n) == QUANTA_OK);
    CHECK(quanta_win_pages_bytes(UINT64_MAX / 4096 + 1, 4096, &n) == QUANTA_INVALID_DATA);
    CHECK(quanta_win_pages_bytes(0, 4096, &n) == QUANTA_OK && n == 0);
    CHECK(quanta_win_pages_bytes(1, 0, &n) == QUANTA_INVALID_DATA);
    CHECK(quanta_win_kelvin_celsius(273.15, &f) == QUANTA_OK && fabs(f) < 1e-12);
    CHECK(quanta_win_kelvin_celsius(310, &f) == QUANTA_OK && fabs(f - 36.85) < 1e-9);
    CHECK(quanta_win_kelvin_celsius(0, &f) == QUANTA_INVALID_DATA);
    CHECK(quanta_win_kelvin_celsius(NAN, &f) == QUANTA_INVALID_DATA);
    CHECK(quanta_win_kelvin_celsius(INFINITY, &f) == QUANTA_INVALID_DATA);
    CHECK(quanta_win_cpu_instance("_Total", &g, &c) == 0);
    CHECK(quanta_win_cpu_instance("0,3", &g, &c) == 1 && g == 0 && c == 3);
    CHECK(quanta_win_cpu_instance("1,63", &g, &c) == 1 && g == 1 && c == 63);
    CHECK(quanta_win_cpu_instance("7", &g, &c) == 1 && g == 0 && c == 7);
    CHECK(quanta_win_cpu_instance("0,_Total", &g, &c) == -1);
    CHECK(quanta_win_cpu_instance("0,64", &g, &c) == -1);
    CHECK(quanta_win_cpu_instance("65536,0", &g, &c) == -1);
    CHECK(quanta_win_cpu_instance("42949672960", &g, &c) == -1);
    CHECK(quanta_win_cpu_instance("-1", &g, &c) == -1);
    CHECK(quanta_win_cpu_instance("0,1#1", &g, &c) == -1);
}

static void test_firmware(void)
{
    builder b;
    uint8_t *r;
    size_t i;
    quanta_smbios *s = (quanta_smbios *)malloc(sizeof(*s));
    const char *bios[] = {"  Firmware Vendor  ", "1.23", "01/01/2025"};
    const char *system[] = {"System Vendor", "System Model", "SYS-123"};
    const char *board[] = {"Board Vendor", "Board Model", "BOARD-123"};
    const char *dimm[] = {"DIMM A1", "BANK 0", "RAM Vendor", "RAM-001", "PART-A"};
    CHECK(s != NULL);
    if (!s)
    {
        return;
    }
    memset(&b, 0, sizeof(b));
    r = record(&b, 0, 0x12, 0);
    r[4] = 1;
    r[5] = 2;
    r[8] = 3;
    strings(&b, bios, 3);
    r = record(&b, 1, 0x1b, 1);
    r[4] = 1;
    r[5] = 2;
    r[7] = 3;
    for (i = 0; i < 16; i++)
    {
        r[8 + i] = (uint8_t)i;
    }
    strings(&b, system, 3);
    r = record(&b, 2, 0x0f, 2);
    r[4] = 1;
    r[5] = 2;
    r[7] = 3;
    strings(&b, board, 3);
    r = record(&b, 17, 0x28, 0x1234);
    put16(r + 12, 16384);
    r[16] = 1;
    r[17] = 2;
    r[23] = 3;
    r[24] = 4;
    r[26] = 5;
    put16(r + 21, 3200);
    strings(&b, dimm, 5);
    end_table(&b);
    CHECK(quanta_smbios_parse(b.bytes, b.size, 3, 2, s) == QUANTA_OK);
    CHECK(s->fields[QUANTA_BIOS_VENDOR].status == QUANTA_OK &&
          !strcmp(s->fields[QUANTA_BIOS_VENDOR].text, "Firmware Vendor"));
    CHECK(!strcmp(s->fields[QUANTA_BOARD_SERIAL].text, "BOARD-123"));
    CHECK(!strcmp(s->fields[QUANTA_SYSTEM_MODEL].text, "System Model"));
    CHECK(!strcmp(s->uuid.text, "03020100-0504-0706-0809-0a0b0c0d0e0f"));
    CHECK(s->dimm_count == 1 && s->dimms[0].size_bytes == UINT64_C(17179869184));
    CHECK(s->dimms[0].handle == 0x1234 && s->dimms[0].speed_mt_s == 3200);
    CHECK(s->dimms[0].serial.status == QUANTA_OK && !strcmp(s->dimms[0].serial.text, "RAM-001"));
    CHECK(quanta_smbios_parse(b.bytes, b.size, 2, 5, s) == QUANTA_OK);
    CHECK(!strcmp(s->uuid.text, "00010203-0405-0607-0809-0a0b0c0d0e0f"));
    {
        uint8_t raw[8200];
        memset(raw, 0, 8);
        raw[1] = 3;
        raw[2] = 2;
        put32(raw + 4, (uint32_t)b.size);
        memcpy(raw + 8, b.bytes, b.size);
        CHECK(quanta_smbios_parse_windows(raw, b.size + 8, s) == QUANTA_OK && s->dimm_count == 1);
        CHECK(quanta_smbios_parse_windows(raw, 7, s) == QUANTA_INVALID_DATA && s->dimm_count == 0);
        put32(raw + 4, UINT32_MAX);
        CHECK(quanta_smbios_parse_windows(raw, b.size + 8, s) == QUANTA_INVALID_DATA &&
              s->fields[QUANTA_BOARD_SERIAL].status == QUANTA_NO_DATA);
    }
    /* Every prefix must be safely handled, including missing double NUL. */
    for (i = 0; i < b.size; i++)
    {
        quanta_status st = quanta_smbios_parse(b.bytes, i, 3, 2, s);
        CHECK(st == QUANTA_OK || st == QUANTA_INVALID_DATA || st == QUANTA_NO_DATA);
        if (st != QUANTA_OK)
        {
            CHECK(s->dimm_count == 0 && s->uuid.status == QUANTA_NO_DATA);
        }
    }
    /* Invalid nonzero index is not silently an empty, successful string. */
    memset(&b, 0, sizeof(b));
    r = record(&b, 2, 8, 2);
    r[4] = 9;
    r[5] = 0;
    strings(&b, board, 3);
    end_table(&b);
    CHECK(quanta_smbios_parse(b.bytes, b.size, 3, 2, s) == QUANTA_OK);
    CHECK(s->fields[QUANTA_BOARD_VENDOR].status == QUANTA_INVALID_DATA);
    CHECK(s->fields[QUANTA_BOARD_MODEL].status == QUANTA_NO_DATA);
    /* No truncation of an oversized firmware string. */
    {
        char long_string[512];
        const char *set[1];
        memset(long_string, 'x', sizeof(long_string) - 1);
        long_string[511] = 0;
        set[0] = long_string;
        memset(&b, 0, sizeof(b));
        r = record(&b, 2, 8, 2);
        r[4] = 1;
        strings(&b, set, 1);
        end_table(&b);
        CHECK(quanta_smbios_parse(b.bytes, b.size, 3, 2, s) == QUANTA_OK &&
              s->fields[QUANTA_BOARD_VENDOR].status == QUANTA_LIMIT);
    }
    /* Type 17 capacities: empty slot, unknown, KiB, extended MiB + speed. */
    memset(&b, 0, sizeof(b));
    record(&b, 17, 23, 1);
    strings(&b, NULL, 0);
    r = record(&b, 17, 23, 2);
    put16(r + 12, 0xffff);
    strings(&b, NULL, 0);
    r = record(&b, 17, 23, 3);
    put16(r + 12, 0x8400);
    strings(&b, NULL, 0);
    r = record(&b, 17, 88, 4);
    put16(r + 12, 0x7fff);
    put32(r + 28, 65536);
    put16(r + 21, 0xffff);
    put32(r + 84, 70000);
    strings(&b, NULL, 0);
    end_table(&b);
    CHECK(quanta_smbios_parse(b.bytes, b.size, 3, 3, s) == QUANTA_OK && s->dimm_count == 3);
    CHECK(s->dimms[0].size_status == QUANTA_NO_DATA);
    CHECK(s->dimms[1].size_bytes == UINT64_C(1048576));
    CHECK(s->dimms[2].size_bytes == UINT64_C(68719476736) && s->dimms[2].speed_mt_s == 70000);
    /* Duplicate DIMM handles would alias different physical resources. */
    memset(&b, 0, sizeof(b));
    r = record(&b, 17, 23, 1);
    put16(r + 12, 1);
    strings(&b, NULL, 0);
    r = record(&b, 17, 23, 1);
    put16(r + 12, 1);
    strings(&b, NULL, 0);
    end_table(&b);
    CHECK(quanta_smbios_parse(b.bytes, b.size, 3, 2, s) == QUANTA_INVALID_DATA &&
          s->dimm_count == 0);
    /* Structural corruption clears even fields parsed before the corruption. */
    memset(&b, 0, sizeof(b));
    r = record(&b, 2, 8, 2);
    r[4] = 1;
    strings(&b, board, 3);
    r = record(&b, 1, 4, 3);
    strings(&b, NULL, 0);
    CHECK(quanta_smbios_parse(b.bytes, b.size, 3, 2, s) == QUANTA_INVALID_DATA);
    CHECK(s->fields[QUANTA_BOARD_VENDOR].status == QUANTA_NO_DATA &&
          !s->fields[QUANTA_BOARD_VENDOR].text[0]);
    CHECK(quanta_smbios_parse(NULL, 0, 3, 2, s) == QUANTA_NO_DATA);
    free(s);
}

static void test_fuzz(void)
{
    quanta_smbios *out = (quanta_smbios *)malloc(sizeof(*out));
    unsigned seed = 0xa53c7201, i, j;
    uint8_t bytes[512];
    CHECK(out != NULL);
    if (!out)
    {
        return;
    }
    for (i = 0; i < 5000; i++)
    {
        size_t n = seed % sizeof(bytes);
        quanta_status status;
        for (j = 0; j < n; j++)
        {
            seed = 1664525u * seed + 1013904223u;
            bytes[j] = (uint8_t)(seed >> 24);
        }
        status = quanta_smbios_parse(bytes, n, 3, 5, out);
        CHECK(status == QUANTA_OK || status == QUANTA_NO_DATA || status == QUANTA_INVALID_DATA ||
              status == QUANTA_LIMIT);
        CHECK(out->dimm_count <= QUANTA_SMBIOS_MAX_DIMMS);
        seed = 1664525u * seed + 1013904223u;
    }
    free(out);
}

int main(void)
{
    test_math();
    test_firmware();
    test_fuzz();
    printf("Windows data (portable): %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
