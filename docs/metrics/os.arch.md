# os.arch

## Linux / исходный провайдер

Machine architecture — semantic version 1.

## Contract

- Type: `string`; kind: `property`.
- Canonical unit: `(unitless string/boolean)`.
- Refresh policy: `INV_REFRESH_ONCE`. Read once and cache until refresh/reappearance.
- Labels: `(none)`.
- Source: uname(), sysconf(), /etc/os-release, /proc/uptime and /proc/loadavg.

## Meaning

uname.machine. This is a machine identifier, not a list of CPU features.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional os.arch;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/filesystems/proc.html

## Windows (0.2)

Источник Windows — Win32 API; `os.arch` — нативная архитектура ОС, а не разрядность EXE.
`os.name` возвращает ProductName из реестра дословно (или Microsoft Windows при его отсутствии).
Строка ProductName может отставать от маркетингового названия; для диагностики смотрите
`os.kernel.release` (RtlGetVersion: major.minor.build). `os.kernel.version` — BuildLabEx,
либо номер NT build и service pack. `os.hostname` — DNS hostname текущего компьютера.
`os.uptime` — GetTickCount64 / 1000, не длительность жизни процесса; быстрый запуск и
гибернация могут сохранять сеанс ядра. `os.cpu.online` — GetActiveProcessorCount для всех групп.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getnativesysteminfo
