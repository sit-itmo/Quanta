# os.memory.total_bytes

## Linux / способ получения

Physical memory — semantic version 1.

## Contract

- Type: `u64`; kind: `gauge`.
- Canonical unit: `B`.
- Refresh policy: `QUANTA_REFRESH_POLL`. Descriptor minimum: 250 ms. Requested period is clamped or rejected in strict mode.
- Labels: `(none)`.
- Source: one /proc/meminfo snapshot.

## Meaning

MemTotal × 1024. The kernel reports kB; INV stores bytes. It is not necessarily installed DIMM capacity.

The procfs view may describe the host rather than this process cgroup. Container limits/quota normalization are outside v0.1.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional os.memory.total_bytes;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/filesystems/proc.html

## Windows backend (требует нативной проверки)

Физические total/available получены одним GlobalMemoryStatusEx; used = total − available,
utilization = used / total. Available включает страницы, которые Windows считает доступными,
а не только полностью свободные страницы; это платформенная семантика, не алгоритм Linux MemAvailable.
Остальные поля берутся одним GetPerformanceInfo. CommitTotal/Limit/Peak — системный commit,
не размер/занятость pagefile. SystemCache, KernelPaged и KernelNonpaged возвращаются в страницах;
умножение на PageSize проверяется на переполнение. ProcessCount, ThreadCount, HandleCount —
системные счётчики объектов, а не значения только процесса INV. Два API не образуют атомарный
снимок всей ОС; формула physical used использует только один ответ GlobalMemoryStatusEx.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getperformanceinfo
