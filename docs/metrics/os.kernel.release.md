# os.kernel.release

## Linux / исходный провайдер

Kernel release — semantic version 1.

## Contract

- Type: `string`; kind: `property`.
- Canonical unit: `(unitless string/boolean)`.
- Refresh policy: `QUANTA_REFRESH_ONCE`. Read once and cache until refresh/reappearance.
- Labels: `(none)`.
- Source: uname(), sysconf(), /etc/os-release, /proc/uptime and /proc/loadavg.

## Meaning

uname.release of the running kernel, not the distribution version.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional os.kernel.release;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/filesystems/proc.html

## Windows backend (требует нативной проверки)

Источник Windows — Win32 API; `os.arch` — нативная архитектура ОС, а не разрядность EXE.
`os.name` возвращает ProductName из реестра дословно (или Microsoft Windows при его отсутствии).
Строка ProductName может отставать от маркетингового названия; для диагностики смотрите
`os.kernel.release` (RtlGetVersion: major.minor.build). `os.kernel.version` — BuildLabEx,
либо номер NT build и service pack. `os.hostname` — DNS hostname текущего компьютера.
`os.uptime` — GetTickCount64 / 1000, не длительность жизни процесса; быстрый запуск и
гибернация могут сохранять сеанс ядра. `os.cpu.online` — GetActiveProcessorCount для всех групп.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getnativesysteminfo
