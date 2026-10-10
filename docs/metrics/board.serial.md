# board.serial

## Linux / исходный провайдер

Board serial number — semantic version 1.

## Contract

- Type: `string`; kind: `property`.
- Canonical unit: `(unitless string/boolean)`.
- Refresh policy: `QUANTA_REFRESH_ONCE`. Read once and cache until refresh/reappearance.
- Labels: `(none)`.
- Source: /sys/class/dmi/id attributes.

## Meaning

DMI board_serial string. Frequently readable only with elevated privileges, sometimes missing or a firmware placeholder. Returned as provided; sensitive output can be masked with --redact.

This metric is QUANTA_SENSITIVE: --redact masks its displayed/exported value, not collection or process memory.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional board.serial;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://github.com/torvalds/linux/blob/master/drivers/firmware/dmi-id.c

## Windows backend (требует нативной проверки)

Данные приходят из GetSystemFirmwareTable(RSMB), без dmidecode, WMI, PowerShell и драйвера.
Парсер ограничивает длины и проверяет структуру, строки и двойные NUL. SMBIOS types 0/1/2/17
дают BIOS, систему, первую baseboard и установленные модули памяти соответственно.
Это сведения, сообщённые прошивкой; их точность, уникальность и наличие не гарантируются.
Заполнители производителя сохраняются дословно; неизвестные поля не становятся нулевыми значениями.
Структурная ошибка всей таблицы не выдаёт ранее разобранные поля как достоверный снимок.
UUID использует порядок байтов SMBIOS 2.6+; all-zero / all-FF означает NO_DATA.
Пустые слоты type 17 не создают DIMM-серий. Метки модулей: handle, locator, bank.
size_bytes — ёмкость установленного устройства, не доступная ОС память; speed_mt_s —
Speed/Extended Speed из type 17 в MT/s (заявленная возможность, не текущая частота и не Configured Speed).
Серийники и UUID имеют флаг SENSITIVE. Идентичность DIMM по handle устойчива только в рамках
текущей таблицы прошивки/сеанса, не является глобальным идентификатором между компьютерами.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemfirmwaretable
