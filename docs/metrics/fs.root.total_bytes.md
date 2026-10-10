# fs.root.total_bytes

## Linux / исходный провайдер

Root filesystem capacity — semantic version 1.

## Contract

- Type: `u64`; kind: `gauge`.
- Canonical unit: `B`.
- Refresh policy: `INV_REFRESH_POLL`. Descriptor minimum: 1000 ms. Requested period is clamped or rejected in strict mode.
- Labels: `(none)`.
- Source: statvfs("/") in the current mount namespace.

## Meaning

f_blocks × f_frsize for / in the current mount namespace.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional fs.root.total_bytes;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://pubs.opengroup.org/onlinepubs/9799919799/functions/statvfs.html

## Windows (0.2)

Под root в Windows понимается том установки Windows, найденный GetWindowsDirectoryW,
а не текущий рабочий каталог и не все диски. GetDiskFreeSpaceExW возвращает квотное представление
для вызывающего пользователя: total = lpTotalNumberOfBytes, available = lpFreeBytesAvailableToCaller,
used = total − available, utilization = used / total. При включённых квотах это не общая занятость
физического тома. В Linux root относится к / и использует statvfs; платформенные формулы описаны
отдельно. Версия 0.2 пока не перечисляет все тома и не собирает SMART.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getdiskfreespaceexw
