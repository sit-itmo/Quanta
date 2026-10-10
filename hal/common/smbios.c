#include "quanta/text.h"
/* SPDX-License-Identifier: MIT */
#include "smbios.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static uint16_t u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void initialize(quanta_smbios *out)
{
    size_t i;
    memset(out, 0, sizeof(*out));
    for (i = 0; i < QUANTA_DMI_FIELDS; i++)
    {
        out->fields[i].status = QUANTA_NO_DATA;
    }
    out->uuid.status = QUANTA_NO_DATA;
}

static void field(quanta_dmi_string *out, const uint8_t *strings, size_t size, unsigned index)
{
    size_t offset = 0, length, start, end;
    unsigned n = 1;
    out->status = QUANTA_NO_DATA;
    out->text[0] = 0;
    if (!index)
    {
        return;
    }
    while (offset < size && strings[offset])
    {
        length = 0;
        while (offset + length < size && strings[offset + length])
        {
            length++;
        }
        if (offset + length >= size)
        {
            out->status = QUANTA_INVALID_DATA;
            return;
        }
        if (n == index)
        {
            start = offset;
            end = offset + length;
            while (start < end && isspace(strings[start]))
            {
                start++;
            }
            while (end > start && isspace(strings[end - 1]))
            {
                end--;
            }
            length = end - start;
            if (!length)
            {
                return;
            }
            if (length >= sizeof(out->text))
            {
                out->status = QUANTA_LIMIT;
                return;
            }
            memcpy(out->text, strings + start, length);
            out->text[length] = 0;
            /* SMBIOS strings may be UTF-8. Sanitize controls/invalid bytes,
               but never silently replace a missing value by an empty success. */
            {
                char clean[QUANTA_SMBIOS_FIELD_CAP];
                quanta_safe_text(clean, sizeof(clean), out->text);
                memcpy(out->text, clean, strlen(clean) + 1);
            }
            out->status = QUANTA_OK;
            return;
        }
        offset += length + 1;
        n++;
    }
    out->status = QUANTA_INVALID_DATA; /* Nonzero index outside the string table. */
}

static void uuid_field(quanta_dmi_string *out, const uint8_t *p, unsigned major, unsigned minor)
{
    size_t i;
    int all0 = 1, allff = 1, modern = major > 2 || (major == 2 && minor >= 6);
    unsigned a[16];
    for (i = 0; i < 16; i++)
    {
        if (p[i])
        {
            all0 = 0;
        }
        if (p[i] != 255)
        {
            allff = 0;
        }
        a[i] = p[i];
    }
    if (all0 || allff)
    {
        out->status = QUANTA_NO_DATA;
        return;
    }
    if (modern)
    {
        a[0] = p[3];
        a[1] = p[2];
        a[2] = p[1];
        a[3] = p[0];
        a[4] = p[5];
        a[5] = p[4];
        a[6] = p[7];
        a[7] = p[6];
    }
    snprintf(out->text, sizeof(out->text),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", a[0], a[1],
             a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14],
             a[15]);
    out->status = QUANTA_OK;
}

static quanta_status parse(const uint8_t *bytes, size_t size, unsigned major, unsigned minor,
                           quanta_smbios *out)
{
    size_t off = 0;
    unsigned seen = 0;
    while (off < size)
    {
        const uint8_t *record = bytes + off, *strings;
        size_t length, end, string_size;
        uint8_t type;
        if (size - off < 4)
        {
            return QUANTA_INVALID_DATA;
        }
        type = record[0];
        length = record[1];
        if (length < 4 || length > size - off)
        {
            return QUANTA_INVALID_DATA;
        }
        end = off + length;
        while (end + 1 < size && !(bytes[end] == 0 && bytes[end + 1] == 0))
        {
            end++;
        }
        if (end + 1 >= size)
        {
            return QUANTA_INVALID_DATA;
        }
        strings = bytes + off + length;
        string_size = end + 1 - (off + length);
        if (type == 0 && !(seen & 1u))
        {
            if (length < 9)
            {
                return QUANTA_INVALID_DATA;
            }
            field(&out->fields[QUANTA_BIOS_VENDOR], strings, string_size, record[4]);
            field(&out->fields[QUANTA_BIOS_VERSION], strings, string_size, record[5]);
            field(&out->fields[QUANTA_BIOS_DATE], strings, string_size, record[8]);
            seen |= 1u;
        }
        else if (type == 1 && !(seen & 2u))
        {
            if (length < 8)
            {
                return QUANTA_INVALID_DATA;
            }
            field(&out->fields[QUANTA_SYSTEM_VENDOR], strings, string_size, record[4]);
            field(&out->fields[QUANTA_SYSTEM_MODEL], strings, string_size, record[5]);
            field(&out->fields[QUANTA_SYSTEM_SERIAL], strings, string_size, record[7]);
            if (length >= 24)
            {
                uuid_field(&out->uuid, record + 8, major, minor);
            }
            seen |= 2u;
        }
        else if (type == 2 && !(seen & 4u))
        {
            if (length < 8)
            {
                return QUANTA_INVALID_DATA;
            }
            field(&out->fields[QUANTA_BOARD_VENDOR], strings, string_size, record[4]);
            field(&out->fields[QUANTA_BOARD_MODEL], strings, string_size, record[5]);
            field(&out->fields[QUANTA_BOARD_SERIAL], strings, string_size, record[7]);
            seen |= 4u;
        }
        else if (type == 17)
        {
            quanta_dmi_dimm *d;
            uint16_t capacity;
            uint32_t speed = 0;
            size_t i;
            if (length < 21)
            {
                return QUANTA_INVALID_DATA;
            }
            capacity = u16(record + 12);
            if (capacity)
            {
                if (out->dimm_count == QUANTA_SMBIOS_MAX_DIMMS)
                {
                    return QUANTA_LIMIT;
                }
                d = &out->dimms[out->dimm_count++];
                d->handle = u16(record + 2);
                for (i = 0; i + 1 < out->dimm_count; i++)
                {
                    if (out->dimms[i].handle == d->handle)
                    {
                        return QUANTA_INVALID_DATA;
                    }
                }
                d->size_status = QUANTA_OK;
                if (capacity == 0xffff)
                {
                    d->size_status = QUANTA_NO_DATA;
                }
                else if (capacity == 0x7fff)
                {
                    if (length < 32)
                    {
                        d->size_status = QUANTA_INVALID_DATA;
                    }
                    else
                    {
                        uint32_t mb = u32(record + 28) & UINT32_C(0x7fffffff);
                        if (!mb)
                        {
                            d->size_status = QUANTA_NO_DATA;
                        }
                        else
                        {
                            d->size_bytes = (uint64_t)mb * UINT64_C(1048576);
                        }
                    }
                }
                else
                {
                    d->size_bytes = (uint64_t)(capacity & 0x7fff) *
                                    (capacity & 0x8000 ? UINT64_C(1024) : UINT64_C(1048576));
                }
                field(&d->locator, strings, string_size, record[16]);
                field(&d->bank, strings, string_size, record[17]);
                field(&d->vendor, strings, string_size, length > 23 ? record[23] : 0);
                field(&d->serial, strings, string_size, length > 24 ? record[24] : 0);
                field(&d->part, strings, string_size, length > 26 ? record[26] : 0);
                if (length >= 23)
                {
                    speed = u16(record + 21);
                }
                if (speed == 0xffff)
                {
                    speed = length >= 88 ? u32(record + 84) : 0;
                }
                d->speed_mt_s = speed;
                d->speed_status = speed ? QUANTA_OK : QUANTA_NO_DATA;
            }
        }
        if (type == 127)
        {
            return QUANTA_OK;
        }
        off = end + 2;
    }
    return QUANTA_OK; /* Some firmware ends at the buffer boundary without type
                         127. */
}

quanta_status quanta_smbios_parse(const uint8_t *bytes, size_t size, unsigned major, unsigned minor,
                                  quanta_smbios *out)
{
    quanta_status status;
    initialize(out);
    if (!bytes || !size)
    {
        return QUANTA_NO_DATA;
    }
    if (size > 16u * 1024u * 1024u)
    {
        return QUANTA_LIMIT;
    }
    status = parse(bytes, size, major, minor, out);
    if (status != QUANTA_OK)
    {
        initialize(out);
    }
    return status;
}

quanta_status quanta_smbios_parse_windows(const uint8_t *bytes, size_t size, quanta_smbios *out)
{
    uint32_t declared;
    initialize(out);
    if (!bytes || size < 8)
    {
        return QUANTA_INVALID_DATA;
    }
    declared = u32(bytes + 4);
    if (declared > size - 8)
    {
        return QUANTA_INVALID_DATA;
    }
    return quanta_smbios_parse(bytes + 8, declared, bytes[1], bytes[2], out);
}
