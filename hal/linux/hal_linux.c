/**
 * @file hal/linux/hal_linux.c
 *
 * Linux HAL: native affinity/info plus a separate device-file backend for physical memory, MSR,
 * PCI and port I/O.
 *
 * HAL: hardware primitives and information about their availability. Metrics and SMBIOS/ACPI
 * parsing are not HAL operations.
 */
/* SPDX-License-Identifier: MIT
 * Linux-native: topology/affinity only. Linux-dev: bounded device-file primitives.
 * No elevation, module loading, SMBIOS parsing or metric collection occurs here.
 */
#include "quanta/osal.h"
#include "hal_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

typedef struct linux_hal
{
    char dev_root[1024];
    char sys_root[1024];
} linux_hal;

static quanta_status errno_status(int error)
{
    switch (error)
    {
    case EACCES: case EPERM: return QUANTA_PERMISSION_DENIED;
    case ENOENT: case ENODEV: case ENXIO: return QUANTA_NOT_PRESENT;
    case ENOMEM: return QUANTA_NO_MEMORY;
    case EINVAL: return QUANTA_INVALID_DATA;
    case ENOSYS: case EOPNOTSUPP: return QUANTA_UNSUPPORTED;
    default: return QUANTA_IO_ERROR;
    }
}

static quanta_status set_affinity(void *data, quanta_cpu_address cpu, quanta_affinity_state *old)
{
    cpu_set_t previous, one;
    (void)data;
    _Static_assert(sizeof(previous) <= sizeof(old->native), "affinity state is too small");
    if (cpu.group || cpu.number >= CPU_SETSIZE) return QUANTA_UNSUPPORTED;
    if (sched_getaffinity(0, sizeof(previous), &previous)) return errno_status(quanta_errno);
    /* Do not escape an existing affinity/cpuset policy. */
    if (!CPU_ISSET(cpu.number, &previous)) return QUANTA_PERMISSION_DENIED;
    CPU_ZERO(&one);
    CPU_SET(cpu.number, &one);
    if (sched_setaffinity(0, sizeof(one), &one)) return errno_status(quanta_errno);
    quanta_memcpy(old->native, &previous, sizeof(previous));
    old->size = sizeof(previous);
    old->owner_thread = (uint64_t)syscall(SYS_gettid);
    old->active = 1;
    return QUANTA_OK;
}

static quanta_status restore_affinity(void *data, quanta_affinity_state *old)
{
    cpu_set_t previous;
    (void)data;
    if (old->size != sizeof(previous) || old->owner_thread != (uint64_t)syscall(SYS_gettid))
        return QUANTA_INVALID_DATA;
    quanta_memcpy(&previous, old->native, sizeof(previous));
    if (sched_setaffinity(0, sizeof(previous), &previous)) return errno_status(quanta_errno);
    quanta_memset(old, 0, sizeof(*old));
    return QUANTA_OK;
}

static quanta_status transfer(const char *path, uint64_t offset, void *out,
                              const void *input, size_t length)
{
    _Static_assert(sizeof(off_t) >= 8, "physical I/O requires 64-bit file offsets");
    int fd, saved_error;
    ssize_t count;
    if (offset > INT64_MAX || length > (uint64_t)INT64_MAX - offset) return QUANTA_INVALID_DATA;
    fd = open(path, (input ? O_WRONLY : O_RDONLY) | O_CLOEXEC);
    if (fd < 0) return errno_status(quanta_errno);
    do
    {
        count = input ? pwrite(fd, input, length, (off_t)offset)
                      : pread(fd, out, length, (off_t)offset);
    } while (count < 0 && quanta_errno == EINTR);
    saved_error = quanta_errno;
    close(fd);
    if (count < 0) return errno_status(saved_error);
    return (size_t)count == length ? QUANTA_OK : QUANTA_IO_ERROR;
}

static uint64_t load_le(const unsigned char *bytes, unsigned length)
{
    uint64_t value = 0;
    unsigned i;
    for (i = 0; i < length; ++i) value |= (uint64_t)bytes[i] << (i * 8);
    return value;
}

static void store_le(unsigned char *bytes, uint64_t value, unsigned length)
{
    unsigned i;
    for (i = 0; i < length; ++i) bytes[i] = (unsigned char)(value >> (i * 8));
}

static quanta_status memory_path(linux_hal *h, char *path, size_t capacity)
{
    int n = quanta_snprintf(path, capacity, "%s/mem", h->dev_root);
    return n >= 0 && (size_t)n < capacity ? QUANTA_OK : QUANTA_LIMIT;
}

static quanta_status read_mem(void *data, uint64_t address, void *out, size_t length)
{
    char path[1100];
    quanta_status s = memory_path((linux_hal *)data, path, sizeof(path));
    return s == QUANTA_OK ? transfer(path, address, out, NULL, length) : s;
}

static quanta_status write_mem(void *data, uint64_t address, const void *input, size_t length)
{
    char path[1100];
    quanta_status s = memory_path((linux_hal *)data, path, sizeof(path));
    return s == QUANTA_OK ? transfer(path, address, NULL, input, length) : s;
}

#if QUANTA_ARCH_X86
static quanta_status msr_transfer(void *data, uint32_t index, uint32_t cpu,
                                 unsigned char bytes[8], int write)
{
    linux_hal *h = (linux_hal *)data;
    char path[1100];
    int n;
    if (cpu == QUANTA_HAL_CURRENT_CPU) return QUANTA_INVALID_DATA;
    n = quanta_snprintf(path, sizeof(path), "%s/cpu/%u/msr", h->dev_root, cpu);
    if (n < 0 || (size_t)n >= sizeof(path)) return QUANTA_LIMIT;
    return transfer(path, index, write ? NULL : bytes, write ? bytes : NULL, 8);
}

static quanta_status read_reg(void *data, quanta_register_kind kind, uint32_t index,
                             uint32_t cpu, uint64_t *out)
{
    unsigned char bytes[8];
    quanta_status s;
    if (kind != QUANTA_REGISTER_MSR) return QUANTA_UNSUPPORTED;
    s = msr_transfer(data, index, cpu, bytes, 0);
    if (s == QUANTA_OK) *out = load_le(bytes, 8);
    return s;
}

static quanta_status write_reg(void *data, quanta_register_kind kind, uint32_t index,
                              uint32_t cpu, uint64_t value)
{
    unsigned char bytes[8];
    if (kind != QUANTA_REGISTER_MSR) return QUANTA_UNSUPPORTED;
    store_le(bytes, value, 8);
    return msr_transfer(data, index, cpu, bytes, 1);
}
#endif

static quanta_status port_transfer(void *data, uint16_t port, unsigned width,
                                  unsigned char bytes[4], int write)
{
    linux_hal *h = (linux_hal *)data;
    char path[1100];
    int n;
    /* /dev/port uses consecutive inb/outb operations, not inw/inl. Do not
     * silently emulate a wider transaction by touching adjacent device ports. */
    if (width != 1) return QUANTA_UNSUPPORTED;
    n = quanta_snprintf(path, sizeof(path), "%s/port", h->dev_root);
    if (n < 0 || (size_t)n >= sizeof(path)) return QUANTA_LIMIT;
    return transfer(path, port, write ? NULL : bytes, write ? bytes : NULL, width);
}

static quanta_status read_portio(void *data, uint16_t port, unsigned width, uint32_t *out)
{
    unsigned char bytes[4];
    quanta_status s = port_transfer(data, port, width, bytes, 0);
    if (s == QUANTA_OK) *out = (uint32_t)load_le(bytes, width);
    return s;
}

static quanta_status write_portio(void *data, uint16_t port, unsigned width, uint32_t value)
{
    unsigned char bytes[4];
    store_le(bytes, value, width);
    return port_transfer(data, port, width, bytes, 1);
}

static quanta_status pci_transfer(void *data, quanta_pci_address a, unsigned width,
                                 unsigned char bytes[4], int write)
{
    linux_hal *h = (linux_hal *)data;
    char path[1200];
    int n = quanta_snprintf(path, sizeof(path), "%s/bus/pci/devices/%04x:%02x:%02x.%x/config",
                     h->sys_root, (unsigned)a.segment, (unsigned)a.bus,
                     (unsigned)a.device, (unsigned)a.function);
    if (n < 0 || (size_t)n >= sizeof(path)) return QUANTA_LIMIT;
    return transfer(path, a.offset, write ? NULL : bytes, write ? bytes : NULL, width);
}

static quanta_status read_pci(void *data, quanta_pci_address a, unsigned width, uint32_t *out)
{
    unsigned char bytes[4];
    quanta_status s = pci_transfer(data, a, width, bytes, 0);
    if (s == QUANTA_OK) *out = (uint32_t)load_le(bytes, width);
    return s;
}

static quanta_status write_pci(void *data, quanta_pci_address a, unsigned width, uint32_t value)
{
    unsigned char bytes[4];
    store_le(bytes, value, width);
    return pci_transfer(data, a, width, bytes, 1);
}

quanta_status quanta_hal_platform_open(const quanta_hal_options *options, quanta_hal **out)
{
    quanta_hal_info info = {0};
    quanta_hal_operations ops = {0};
    linux_hal *h = NULL;
    quanta_status s;
    long cpus, pages, page_size;
    const char *name = options->name;
    int devices = name && !quanta_strcmp(name, "linux-dev");
    if (name && *name && quanta_strcmp(name, "native") && quanta_strcmp(name, "linux-native") && !devices)
        return QUANTA_UNSUPPORTED;
    quanta_strcpy(info.name, devices ? "linux-dev" : "linux-native");
    info.maximum_physical_address = INT64_MAX - 1;
    cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus > 0 && (unsigned long)cpus <= UINT32_MAX)
    {
        info.logical_cpus = (uint32_t)cpus;
        info.known |= QUANTA_HAL_CPU_COUNT_KNOWN;
    }
    pages = sysconf(_SC_PHYS_PAGES);
    page_size = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0 && (uint64_t)pages <= UINT64_MAX / (uint64_t)page_size)
    {
        info.physical_memory_bytes = (uint64_t)pages * (uint64_t)page_size;
        info.known |= QUANTA_HAL_MEMORY_SIZE_KNOWN;
    }
    ops.set_affinity = set_affinity;
    ops.restore_affinity = restore_affinity;
    if (devices)
    {
        const char *dev = options->dev_root ? options->dev_root : "/dev";
        const char *sys = options->sys_root ? options->sys_root : "/sys";
        h = (linux_hal *)quanta_calloc(1, sizeof(*h));
        if (!h) return QUANTA_NO_MEMORY;
        if (quanta_strlen(dev) >= sizeof(h->dev_root) || quanta_strlen(sys) >= sizeof(h->sys_root))
        {
            quanta_free(h);
            return QUANTA_LIMIT;
        }
        quanta_strcpy(h->dev_root, dev);
        quanta_strcpy(h->sys_root, sys);
        ops.close = quanta_free;
        ops.read_mem = read_mem;
        ops.write_mem = write_mem;
        ops.read_portio = read_portio;
        ops.write_portio = write_portio;
        ops.read_pci = read_pci;
        ops.write_pci = write_pci;
#if QUANTA_ARCH_X86
        ops.read_reg = read_reg;
        ops.write_reg = write_reg;
        info.register_kinds = 1u << QUANTA_REGISTER_MSR;
#endif
    }
    s = quanta_hal_attach(&info, &ops, h, options, out);
    if (s != QUANTA_OK) quanta_free(h);
    return s;
}
