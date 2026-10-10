# cpu.core.utilization

## Linux / исходный провайдер

CPU busy fraction — semantic version 1.

## Contract

- Type: `f64`; kind: `gauge`.
- Canonical unit: `1`.
- Refresh policy: `QUANTA_REFRESH_POLL`. Descriptor minimum: 100 ms. Requested period is clamped or rejected in strict mode.
- Labels: `cpu`.
- Source: one /proc/stat snapshot, first eight CPU time fields only.

## Meaning

Same utilization formula as cpu.utilization, separately for each cpuN row. Label cpu is the logical CPU number. First observation and decreasing accounting fields yield WARMUP.

First utilization sample is WARMUP. A decreasing field (including iowait), no delta, or a new instance resets the baseline; impossible negative utilization is never emitted.

The procfs view may describe the host rather than this process cgroup. Container limits/quota normalization are outside v0.1.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional cpu.core.utilization;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/filesystems/proc.html

## Windows backend (требует нативной проверки)

На системе с одной процессорной группой GetSystemTimes даёт aggregate kernel + user;
idle уже входит в kernel и второй раз не добавляется. Загрузка вычисляется по двум снимкам:
1 − Δidle / Δ(kernel+user). Начальный результат и сбросы — WARMUP.
Единица сырых cpu.time_total / cpu.idle_total — 100-нс ticks; частота явно задана
windows.cpu.tick_frequency_hz = 10 000 000. Это не USER_HZ Linux.
На системах с несколькими группами сырые aggregate tick-метрики возвращают UNSUPPORTED,
а общая и поядерная загрузка используют PDH Processor Information / % Processor Time.
Поядерная загрузка всегда берётся из прямых PDH fractions, не из искусственно накопленных ticks.
Метки cpu.core.utilization: cpu="group,number", group и number. Локализация PDH выполняется
через PdhAddEnglishCounterW и локализованный wildcard; имена групп не зависят от языка интерфейса.
Метрика измеряет долю занятого процессорного времени, а не Processor Utility с поправкой на частоту.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes
