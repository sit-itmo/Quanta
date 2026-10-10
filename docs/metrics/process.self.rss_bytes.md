# process.self.rss_bytes

## Linux / исходный провайдер

Collector resident memory — semantic version 1.

## Contract

- Type: `u64`; kind: `gauge`.
- Canonical unit: `B`.
- Refresh policy: `INV_REFRESH_POLL`. Descriptor minimum: 250 ms. Requested period is clamped or rejected in strict mode.
- Labels: `(none)`.
- Source: /proc/self/statm resident pages × sysconf(_SC_PAGESIZE).

## Meaning

Resident pages from /proc/self/statm times page size. A low-cost, approximate current resident footprint of inv, not PSS, allocator bytes, peak RSS or another process.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional process.self.rss_bytes;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/filesystems/proc.html

## Windows (0.2)

Измеряется только сам процесс INV, не любой произвольный PID.
rss_bytes = WorkingSetSize, private_bytes = PrivateUsage (private committed bytes),
handles = GetProcessHandleCount, pid = GetCurrentProcessId. Working set и private commit
не являются одним и тем же показателем и не должны складываться.
io.read_bytes_total / io.write_bytes_total — ReadTransferCount/WriteTransferCount из
GetProcessIoCounters: это учтённый процессный I/O, не обещание исключительно дисковой нагрузки.
Значения поступают из нескольких WinAPI, не из атомарного меж-API снимка.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex
