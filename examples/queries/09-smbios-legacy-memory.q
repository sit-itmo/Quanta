# Explicit privileged backend. No elevation or automatic fallback.
# Legacy firmware window only; unsupported on many UEFI-only/locked-down systems.
version 1;
hal "linux-dev";
once;
select optional smbios.legacy_mem.board.model;
select optional smbios.legacy_mem.bios.vendor;
