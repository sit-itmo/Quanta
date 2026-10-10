/* SPDX-License-Identifier: MIT */
#include "hal_internal.h"

struct quanta_hal
{
    quanta_hal_info info;
    quanta_hal_operations operations;
    void *private_data;
    quanta_physical_range ranges[QUANTA_HAL_MAX_RANGES];
    size_t range_count;
};

static uint32_t operation_bits(const quanta_hal_operations *o)
{
    uint32_t bits = 0;
    if (o->read_reg) bits |= QUANTA_HAL_READ_REG;
    if (o->write_reg) bits |= QUANTA_HAL_WRITE_REG;
    if (o->read_mem) bits |= QUANTA_HAL_READ_MEM;
    if (o->write_mem) bits |= QUANTA_HAL_WRITE_MEM;
    if (o->read_portio) bits |= QUANTA_HAL_READ_PORTIO;
    if (o->write_portio) bits |= QUANTA_HAL_WRITE_PORTIO;
    if (o->read_pci) bits |= QUANTA_HAL_READ_PCI;
    if (o->write_pci) bits |= QUANTA_HAL_WRITE_PCI;
    if (o->read_gpio) bits |= QUANTA_HAL_READ_GPIO;
    if (o->write_gpio) bits |= QUANTA_HAL_WRITE_GPIO;
    if (o->set_affinity && o->restore_affinity) bits |= QUANTA_HAL_AFFINITY;
    return bits;
}

quanta_status quanta_hal_attach(const quanta_hal_info *info, const quanta_hal_operations *ops,
                               void *data, const quanta_hal_options *options, quanta_hal **out)
{
    quanta_hal *h;
    size_t i;
    if (!out) return QUANTA_INVALID_DATA;
    *out = NULL;
    if (!info || !ops || !info->name[0] || !memchr(info->name, 0, sizeof(info->name)) ||
        (options && (options->flags & ~QUANTA_HAL_ALLOW_WRITE))) return QUANTA_INVALID_DATA;
    if (options && (options->physical_range_count > QUANTA_HAL_MAX_RANGES ||
        (options->physical_range_count && !options->physical_ranges))) return QUANTA_INVALID_DATA;
    if (options)
    {
        for (i = 0; i < options->physical_range_count; ++i)
        {
            const quanta_physical_range *r = &options->physical_ranges[i];
            if (!r->size || r->size - 1 > UINT64_MAX - r->base) return QUANTA_INVALID_DATA;
        }
    }
    h = (quanta_hal *)calloc(1, sizeof(*h));
    if (!h) return QUANTA_NO_MEMORY;
    h->info = *info;
    h->info.capabilities = operation_bits(ops);
    if (!ops->read_reg && !ops->write_reg) h->info.register_kinds = 0;
    h->info.maximum_transfer = info->maximum_transfer ? info->maximum_transfer : QUANTA_HAL_MAX_TRANSFER;
    if (h->info.maximum_transfer > QUANTA_HAL_MAX_TRANSFER) h->info.maximum_transfer = QUANTA_HAL_MAX_TRANSFER;
    h->info.writes_enabled = options && (options->flags & QUANTA_HAL_ALLOW_WRITE) ? 1 : 0;
    h->operations = *ops;
    h->private_data = data;
    if (options && options->physical_range_count)
    {
        h->range_count = options->physical_range_count;
        memcpy(h->ranges, options->physical_ranges, h->range_count * sizeof(h->ranges[0]));
    }
    *out = h;
    return QUANTA_OK;
}

quanta_status quanta_hal_open(const quanta_hal_options *options, quanta_hal **out)
{
    quanta_hal_options defaults = {0};
    if (!out) return QUANTA_INVALID_DATA;
    *out = NULL;
    if (!options) options = &defaults;
    if (options->name && !strcmp(options->name, "none"))
    {
        quanta_hal_info info = {0};
        quanta_hal_operations ops = {0};
        memcpy(info.name, "none", 5);
        return quanta_hal_attach(&info, &ops, NULL, options, out);
    }
    return quanta_hal_platform_open(options, out);
}

void quanta_hal_close(quanta_hal *h)
{
    if (!h) return;
    if (h->operations.close) h->operations.close(h->private_data);
    free(h);
}

quanta_status quanta_hal_get_info(const quanta_hal *h, quanta_hal_info *out)
{
    if (!h || !out) return QUANTA_INVALID_DATA;
    *out = h->info;
    return QUANTA_OK;
}

const char *const *quanta_hal_names(size_t *count)
{
#if QUANTA_OS_LINUX
    static const char *const names[] = {"none", "native", "linux-native", "linux-dev"};
#elif QUANTA_OS_WINDOWS
    static const char *const names[] = {"none", "native", "windows-native"};
#else
    static const char *const names[] = {"none", "native", "esp8266-native"};
#endif
    if (count) *count = sizeof(names) / sizeof(names[0]);
    return names;
}

static quanta_status reg_check(const quanta_hal *h, quanta_register_kind kind)
{
    if (!h || (unsigned)kind >= QUANTA_REGISTER_KIND_COUNT) return QUANTA_INVALID_DATA;
    return h->info.register_kinds & (1u << kind) ? QUANTA_OK : QUANTA_UNSUPPORTED;
}

quanta_status quanta_hal_read_reg(quanta_hal *h, quanta_register_kind kind, uint32_t index,
                                uint32_t cpu, uint64_t *out)
{
    quanta_status s;
    if (!out) return QUANTA_INVALID_DATA;
    *out = 0;
    s = reg_check(h, kind);
    if (s != QUANTA_OK) return s;
    if (!h->operations.read_reg) return QUANTA_UNSUPPORTED;
    s = h->operations.read_reg(h->private_data, kind, index, cpu, out);
    if (s != QUANTA_OK) *out = 0;
    return s;
}

quanta_status quanta_hal_write_reg(quanta_hal *h, quanta_register_kind kind, uint32_t index,
                                 uint32_t cpu, uint64_t value)
{
    quanta_status s = reg_check(h, kind);
    if (s != QUANTA_OK) return s;
    if (!h->operations.write_reg) return QUANTA_UNSUPPORTED;
    if (!h->info.writes_enabled) return QUANTA_PERMISSION_DENIED;
    return h->operations.write_reg(h->private_data, kind, index, cpu, value);
}

static quanta_status memory_check(quanta_hal *h, uint64_t address, const void *buffer, size_t length)
{
    size_t i;
    if (!h || !buffer || !length) return QUANTA_INVALID_DATA;
    if (length > h->info.maximum_transfer) return QUANTA_LIMIT;
    if ((uint64_t)(length - 1) > UINT64_MAX - address) return QUANTA_INVALID_DATA;
    if (address > h->info.maximum_physical_address ||
        (uint64_t)(length - 1) > h->info.maximum_physical_address - address) return QUANTA_INVALID_DATA;
    if (!h->range_count) return QUANTA_OK;
    for (i = 0; i < h->range_count; ++i)
    {
        const quanta_physical_range *r = &h->ranges[i];
        if (address >= r->base && address - r->base < r->size &&
            length <= r->size - (address - r->base)) return QUANTA_OK;
    }
    return QUANTA_PERMISSION_DENIED;
}

quanta_status quanta_hal_read_mem(quanta_hal *h, uint64_t address, void *out, size_t length)
{
    quanta_status s;
    if (!h || !out || !length) return QUANTA_INVALID_DATA;
    if (!h->operations.read_mem) return QUANTA_UNSUPPORTED;
    s = memory_check(h, address, out, length);
    if (s != QUANTA_OK) return s;
    s = h->operations.read_mem(h->private_data, address, out, length);
    if (s != QUANTA_OK) memset(out, 0, length);
    return s;
}

quanta_status quanta_hal_write_mem(quanta_hal *h, uint64_t address, const void *data, size_t length)
{
    quanta_status s;
    if (!h || !data || !length) return QUANTA_INVALID_DATA;
    if (!h->operations.write_mem) return QUANTA_UNSUPPORTED;
    s = memory_check(h, address, data, length);
    if (s != QUANTA_OK) return s;
    if (!h->info.writes_enabled) return QUANTA_PERMISSION_DENIED;
    return h->operations.write_mem(h->private_data, address, data, length);
}

static int valid_width(unsigned width)
{
    return width == 1 || width == 2 || width == 4;
}

static int value_fits(unsigned width, uint32_t value)
{
    return width == 4 || value < (1u << (width * 8));
}

quanta_status quanta_hal_read_portio(quanta_hal *h, uint16_t port, unsigned width, uint32_t *out)
{
    quanta_status s;
    if (!out) return QUANTA_INVALID_DATA;
    *out = 0;
    if (!h || !valid_width(width) || (uint32_t)port + width > 65536u) return QUANTA_INVALID_DATA;
    if (!h->operations.read_portio) return QUANTA_UNSUPPORTED;
    s = h->operations.read_portio(h->private_data, port, width, out);
    if (s == QUANTA_OK && !value_fits(width, *out)) s = QUANTA_INVALID_DATA;
    if (s != QUANTA_OK) *out = 0;
    return s;
}

quanta_status quanta_hal_write_portio(quanta_hal *h, uint16_t port, unsigned width, uint32_t value)
{
    if (!h || !valid_width(width) || (uint32_t)port + width > 65536u || !value_fits(width, value))
        return QUANTA_INVALID_DATA;
    if (!h->operations.write_portio) return QUANTA_UNSUPPORTED;
    if (!h->info.writes_enabled) return QUANTA_PERMISSION_DENIED;
    return h->operations.write_portio(h->private_data, port, width, value);
}

static int valid_pci(quanta_pci_address a, unsigned width)
{
    return valid_width(width) && a.device <= 31 && a.function <= 7 &&
           a.offset < 4096 && a.offset + width <= 4096 && a.offset % width == 0;
}

quanta_status quanta_hal_read_pci(quanta_hal *h, quanta_pci_address a, unsigned width, uint32_t *out)
{
    quanta_status s;
    if (!out) return QUANTA_INVALID_DATA;
    *out = 0;
    if (!h || !valid_pci(a, width)) return QUANTA_INVALID_DATA;
    if (!h->operations.read_pci) return QUANTA_UNSUPPORTED;
    s = h->operations.read_pci(h->private_data, a, width, out);
    if (s == QUANTA_OK && !value_fits(width, *out)) s = QUANTA_INVALID_DATA;
    if (s != QUANTA_OK) *out = 0;
    return s;
}

quanta_status quanta_hal_write_pci(quanta_hal *h, quanta_pci_address a, unsigned width, uint32_t value)
{
    if (!h || !valid_pci(a, width) || !value_fits(width, value)) return QUANTA_INVALID_DATA;
    if (!h->operations.write_pci) return QUANTA_UNSUPPORTED;
    if (!h->info.writes_enabled) return QUANTA_PERMISSION_DENIED;
    return h->operations.write_pci(h->private_data, a, width, value);
}

quanta_status quanta_hal_read_gpio(quanta_hal *h, uint32_t controller, uint32_t pin, int *out)
{
    quanta_status s;
    if (!out) return QUANTA_INVALID_DATA;
    *out = 0;
    if (!h) return QUANTA_INVALID_DATA;
    if (!h->operations.read_gpio) return QUANTA_UNSUPPORTED;
    s = h->operations.read_gpio(h->private_data, controller, pin, out);
    if (s == QUANTA_OK && *out != 0 && *out != 1) s = QUANTA_INVALID_DATA;
    if (s != QUANTA_OK) *out = 0;
    return s;
}

quanta_status quanta_hal_write_gpio(quanta_hal *h, uint32_t controller, uint32_t pin, int value)
{
    if (!h || (value != 0 && value != 1)) return QUANTA_INVALID_DATA;
    if (!h->operations.write_gpio) return QUANTA_UNSUPPORTED;
    if (!h->info.writes_enabled) return QUANTA_PERMISSION_DENIED;
    return h->operations.write_gpio(h->private_data, controller, pin, value);
}

quanta_status quanta_hal_set_affinity(quanta_hal *h, quanta_cpu_address cpu, quanta_affinity_state *old)
{
    if (!h || !old || old->active) return QUANTA_INVALID_DATA;
    if (!h->operations.set_affinity || !h->operations.restore_affinity) return QUANTA_UNSUPPORTED;
    memset(old, 0, sizeof(*old));
    return h->operations.set_affinity(h->private_data, cpu, old);
}

quanta_status quanta_hal_restore_affinity(quanta_hal *h, quanta_affinity_state *old)
{
    if (!h || !old || !old->active || old->size > sizeof(old->native)) return QUANTA_INVALID_DATA;
    if (!h->operations.restore_affinity) return QUANTA_UNSUPPORTED;
    return h->operations.restore_affinity(h->private_data, old);
}
