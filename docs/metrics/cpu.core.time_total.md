# cpu.core.time_total

CPU accounted ticks — semantic version 1.

## Contract

- Type: `u64`; kind: `counter`.
- Canonical unit: `ticks`.
- Refresh policy: `INV_REFRESH_POLL`. Descriptor minimum: 100 ms. Requested period is clamped or rejected in strict mode.
- Labels: `cpu`.
- Source: one /proc/stat snapshot, first eight CPU time fields only.

## Meaning

Sum of the first eight cpuN accounting fields in USER_HZ ticks. Series identity is the observed logical CPU row, not a physical core serial.

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
mode file; format table; once; select optional cpu.core.time_total;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/filesystems/proc.html
