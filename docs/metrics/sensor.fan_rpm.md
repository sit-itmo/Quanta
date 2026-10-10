# sensor.fan_rpm

Fan speed channel — semantic version 1.

## Contract

- Type: `u64`; kind: `gauge`.
- Canonical unit: `rpm`.
- Refresh policy: `QUANTA_REFRESH_POLL`. Descriptor minimum: 1000 ms. Requested period is clamped or rejected in strict mode.
- Labels: `chip,channel,label,device`.
- Source: /sys/class/hwmon/hwmon*/tempN_input or fanN_input.

## Meaning

Standard fanN_input integer RPM. Zero can legitimately mean stopped. Nonzero optional fanN_fault yields NO_DATA. Missing label is replaced by the channel name, not a guessed physical fan identity.

Runtime update_interval, when present and valid, raises the polling minimum. Device labels are descriptive, not universally unique IDs. There is no privileged MSR/NVML fallback.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional sensor.fan_rpm;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/hwmon/sysfs-interface.html
