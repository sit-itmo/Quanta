/* SPDX-License-Identifier: MIT
 * Hardware primitives only. No metric, SMBIOS, ACPI, query or frontend types.
 */
#ifndef QUANTA_HAL_H
#define QUANTA_HAL_H
#include "quanta/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QUANTA_HAL_MAX_TRANSFER (1024u * 1024u)
#define QUANTA_HAL_MAX_RANGES 16u
#define QUANTA_HAL_CURRENT_CPU UINT32_MAX

typedef struct quanta_hal quanta_hal;

typedef enum quanta_register_kind
{
    QUANTA_REGISTER_MSR,
    QUANTA_REGISTER_CONTROL,
    QUANTA_REGISTER_DEBUG,
    QUANTA_REGISTER_CYCLE,
    QUANTA_REGISTER_KIND_COUNT
} quanta_register_kind;

enum
{
    QUANTA_HAL_READ_REG       = 1u << 0,
    QUANTA_HAL_WRITE_REG      = 1u << 1,
    QUANTA_HAL_READ_MEM       = 1u << 2,
    QUANTA_HAL_WRITE_MEM      = 1u << 3,
    QUANTA_HAL_READ_PORTIO    = 1u << 4,
    QUANTA_HAL_WRITE_PORTIO   = 1u << 5,
    QUANTA_HAL_READ_PCI       = 1u << 6,
    QUANTA_HAL_WRITE_PCI      = 1u << 7,
    QUANTA_HAL_READ_GPIO      = 1u << 8,
    QUANTA_HAL_WRITE_GPIO     = 1u << 9,
    QUANTA_HAL_AFFINITY       = 1u << 10
};

enum
{
    QUANTA_HAL_ALLOW_WRITE = 1u << 0,
    QUANTA_HAL_MEMORY_SIZE_KNOWN = 1u << 0,
    QUANTA_HAL_CPU_COUNT_KNOWN = 1u << 1
};

typedef struct quanta_physical_range
{
    uint64_t base;
    uint64_t size;
} quanta_physical_range;

typedef struct quanta_pci_address
{
    uint16_t segment;
    uint8_t bus, device, function;
    uint16_t offset;
} quanta_pci_address;

typedef struct quanta_cpu_address
{
    uint16_t group;  /* Linux/ESP: zero. Windows: processor group. */
    uint16_t number; /* CPU number within the group. */
} quanta_cpu_address;

typedef struct quanta_affinity_state
{
    uint64_t owner_thread;
    size_t size;
    unsigned char native[256];
    int active;
} quanta_affinity_state;

typedef struct quanta_hal_info
{
    char name[48];
    uint32_t capabilities;   /* Implemented operations, NOT a promise of OS permission. */
    uint32_t register_kinds; /* 1u << quanta_register_kind. */
    uint32_t known;          /* *_KNOWN bits: zero must not masquerade as known data. */
    uint32_t logical_cpus;
    uint64_t physical_memory_bytes; /* Installed/OS-reported capacity, not a readable map. */
    uint64_t maximum_physical_address;
    uint32_t maximum_transfer;
    int writes_enabled;
} quanta_hal_info;

typedef struct quanta_hal_options
{
    const char *name;     /* NULL/empty: native; "none" disables all hardware primitives. */
    unsigned flags;       /* Writes are disabled unless ALLOW_WRITE is explicitly set. */
    const char *dev_root; /* Linux device nodes; default /dev. Used by tests/embedding. */
    const char *sys_root; /* Linux PCI config space; default /sys. */
    /* Optional application allowlist. With zero ranges, Linux device permissions
     * govern physical access. Every request must fit wholly in one supplied range.
     * This is a restriction, not discovery of safe RAM/MMIO. The list is copied. */
    const quanta_physical_range *physical_ranges;
    size_t physical_range_count;
} quanta_hal_options;

/* Low-level backend contract; also the injection seam for unit-test HALs.
 * The wrapper copies this vtable and info, but owns private_data only through close.
 * All I/O must be bounded; no callbacks may execute a query or decode SMBIOS/ACPI.
 * Device writes may have side effects even on failure and are never transactional. */
typedef struct quanta_hal_operations
{
    void (*close)(void *private_data);
    quanta_status (*read_reg)(void *, quanta_register_kind, uint32_t index, uint32_t cpu, uint64_t *);
    quanta_status (*write_reg)(void *, quanta_register_kind, uint32_t index, uint32_t cpu, uint64_t);
    quanta_status (*read_mem)(void *, uint64_t physical_address, void *, size_t);
    quanta_status (*write_mem)(void *, uint64_t physical_address, const void *, size_t);
    quanta_status (*read_portio)(void *, uint16_t port, unsigned width, uint32_t *);
    quanta_status (*write_portio)(void *, uint16_t port, unsigned width, uint32_t);
    quanta_status (*read_pci)(void *, quanta_pci_address, unsigned width, uint32_t *);
    quanta_status (*write_pci)(void *, quanta_pci_address, unsigned width, uint32_t);
    quanta_status (*read_gpio)(void *, uint32_t controller, uint32_t pin, int *);
    quanta_status (*write_gpio)(void *, uint32_t controller, uint32_t pin, int);
    quanta_status (*set_affinity)(void *, quanta_cpu_address, quanta_affinity_state *);
    quanta_status (*restore_affinity)(void *, quanta_affinity_state *);
} quanta_hal_operations;

quanta_status quanta_hal_open(const quanta_hal_options *, quanta_hal **out);
quanta_status quanta_hal_attach(const quanta_hal_info *, const quanta_hal_operations *,
                               void *private_data, const quanta_hal_options *, quanta_hal **out);
void quanta_hal_close(quanta_hal *);
quanta_status quanta_hal_get_info(const quanta_hal *, quanta_hal_info *out);
const char *const *quanta_hal_names(size_t *count);

quanta_status quanta_hal_read_reg(quanta_hal *, quanta_register_kind, uint32_t, uint32_t, uint64_t *);
quanta_status quanta_hal_write_reg(quanta_hal *, quanta_register_kind, uint32_t, uint32_t, uint64_t);
quanta_status quanta_hal_read_mem(quanta_hal *, uint64_t, void *, size_t);
quanta_status quanta_hal_write_mem(quanta_hal *, uint64_t, const void *, size_t);
quanta_status quanta_hal_read_portio(quanta_hal *, uint16_t, unsigned, uint32_t *);
quanta_status quanta_hal_write_portio(quanta_hal *, uint16_t, unsigned, uint32_t);
quanta_status quanta_hal_read_pci(quanta_hal *, quanta_pci_address, unsigned, uint32_t *);
quanta_status quanta_hal_write_pci(quanta_hal *, quanta_pci_address, unsigned, uint32_t);
quanta_status quanta_hal_read_gpio(quanta_hal *, uint32_t, uint32_t, int *);
quanta_status quanta_hal_write_gpio(quanta_hal *, uint32_t, uint32_t, int);
quanta_status quanta_hal_set_affinity(quanta_hal *, quanta_cpu_address, quanta_affinity_state *);
quanta_status quanta_hal_restore_affinity(quanta_hal *, quanta_affinity_state *);

/* Read results are defined only on OK; wrappers clear failed scalar reads.
 * Register CPU is a flat logical number for /dev/cpu/N/msr. CURRENT_CPU is not
 * accepted there: migration would otherwise make an implicit CPU ambiguous.
 * Port/PCI width is 1, 2 or 4 bytes. Physical I/O does not promise MMIO atomicity.
 * Handles and affinity tokens are single-owner; serialize access externally. */
#ifdef __cplusplus
}
#endif
#endif
