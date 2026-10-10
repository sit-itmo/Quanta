# cpu.brand

## Linux / способ получения

CPU brand string — semantic version 1.

## Contract

- Type: `string`; kind: `property`.
- Canonical unit: `(unitless string/boolean)`.
- Refresh policy: `QUANTA_REFRESH_ONCE`. Read once and cache until refresh/reappearance.
- Labels: `(none)`.
- Source: CPUID instruction on one affinity-pinned allowed logical CPU.

## Meaning

Extended CPUID brand string, if supported, on cpu.cpuid_reference.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional cpu.brand;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

## Windows backend (требует нативной проверки)

Используется прямой CPUID на одном допустимом reference CPU с временной group affinity,
которая восстанавливается после чтения. cpu.cpuid_reference = (group << 16) | processor_number.
Это не пересечение/объединение возможностей всех CPU, особенно на гетерогенных машинах.
VMX/SVM и SIMD bits означают аппаратно сообщённую возможность; VMX не доказывает разрешение
виртуализации в прошивке, а AVX/AVX2 — готовность ОС сохранять расширенное состояние.
Внутри VM читается то, что экспонирует гипервизор. На не-x86 архитектуре провайдер исключён.

Описание API: https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex
