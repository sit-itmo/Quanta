# os.swap.total_bytes

Swap capacity — semantic version 1.

## Contract

- Type: `u64`; kind: `gauge`.
- Canonical unit: `B`.
- Refresh policy: `QUANTA_REFRESH_POLL`. Descriptor minimum: 250 ms. Requested period is clamped or rejected in strict mode.
- Labels: `(none)`.
- Source: one /proc/meminfo snapshot.

## Meaning

SwapTotal × 1024; zero is valid on a system without swap.

The procfs view may describe the host rather than this process cgroup. Container limits/quota normalization are outside v0.1.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional os.swap.total_bytes;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/filesystems/proc.html
