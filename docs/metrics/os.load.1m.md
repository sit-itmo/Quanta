# os.load.1m

Load average (1 minute) — semantic version 1.

## Contract

- Type: `f64`; kind: `gauge`.
- Canonical unit: `(unitless string/boolean)`.
- Refresh policy: `QUANTA_REFRESH_POLL`. Descriptor minimum: 1000 ms. Requested period is clamped or rejected in strict mode.
- Labels: `(none)`.
- Source: uname(), sysconf(), /etc/os-release, /proc/uptime and /proc/loadavg.

## Meaning

First field of /proc/loadavg. This is one-minute load average, not CPU utilization or a percentage.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional os.load.1m;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/filesystems/proc.html
