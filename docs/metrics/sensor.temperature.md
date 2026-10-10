# sensor.temperature

## Linux / способ получения

Temperature channel — semantic version 1.

## Contract

- Type: `f64`; kind: `gauge`.
- Canonical unit: `degC`.
- Refresh policy: `QUANTA_REFRESH_POLL`. Descriptor minimum: 1000 ms. Requested period is clamped or rejected in strict mode.
- Labels: `chip,channel,label,device`.
- Source: /sys/class/hwmon/hwmon*/tempN_input or fanN_input.

## Meaning

Raw standard tempN_input divided by 1000 into degrees Celsius. Check chip/channel/label/device. A temp1 channel is not automatically a CPU temperature. Driver-specific voltage-scaled or board-calibrated sensors may require configuration not implemented here. Nonzero optional tempN_fault yields NO_DATA.

Runtime update_interval, when present and valid, raises the polling minimum. Device labels are descriptive, not universally unique IDs. There is no privileged MSR/NVML fallback.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional sensor.temperature;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/hwmon/sysfs-interface.html

## Windows backend (требует нативной проверки)

Windows-провайдер использует PDH Thermal Zone Information / Temperature только при наличии
этих счётчиков. Документированная единица Temperature — kelvin; INV возвращает kelvin − 273.15
в degC. Это НЕ raw ACPI _TMP в десятых kelvin. Невалидные/нулевые значения отклоняются.
Зона не объявляется CPU package/core: соответствие компонентов из этого источника неизвестно.
На многих машинах соответствующие counters/instances отсутствуют; optional-запрос продолжает
работать, а не рисует выдуманные 0 °C. Не устанавливается драйвер чтения MSR/SMBus/EC.
Метки: device, chip="acpi_thermal_zone", channel="temperature", label. Опрос не чаще 1 секунды;
это предел программы, не гарантия обновления самого датчика каждую секунду.

Описание API: https://learn.microsoft.com/en-us/windows-hardware/design/device-experiences/examples--requirements-and-diagnostics
