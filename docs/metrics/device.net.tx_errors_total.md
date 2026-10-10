# device.net.tx_errors_total

## Linux / способ получения

Transmit errors — semantic version 1.

## Contract

- Type: `u64`; kind: `counter`.
- Canonical unit: `errors`.
- Refresh policy: `QUANTA_REFRESH_POLL`. Descriptor minimum: 100 ms. Requested period is clamped or rejected in strict mode.
- Labels: `ifname,ifindex`.
- Source: /sys/class/net identity and attributes; one /proc/net/dev counters snapshot.

## Meaning

Cumulative transmit errors from /proc/net/dev for this interface. These are kernel link statistics, not application payload throughput. Counters may reset when the interface is recreated. Use rate(device.net.tx_errors_total) for per-second values or delta(device.net.tx_errors_total) for changes.

Identity is ifindex within the current network namespace; display name is not used alone. Reuse of the same ifindex is not guaranteed to be detected. Rediscovery is approximately every 5 s or explicit refresh. Very fast remove/recreate cycles can be missed by any periodic enumeration.

## Errors and unavailable values

A successful zero/false/empty string is distinct from an unsuccessful read.
PERMISSION_DENIED reports access restrictions; NOT_PRESENT/GONE report missing
resources; INVALID_DATA reports malformed/range-invalid input; NO_DATA reports
missing required source fields. The last successful sample may be displayed
with its original age, never as a newly successful measurement.

## Query

```text
mode file; format table; once; select optional device.net.tx_errors_total;
```

For a derived rate or CPU utilization, allow a baseline with `--warmup 1200ms`
or run a periodic query. Actual minimums can require a longer interval.

## Reference

https://docs.kernel.org/networking/statistics.html

## Windows backend (требует нативной проверки)

Один GetIfTable2 обслуживает все выбранные интерфейсы. Из user-facing discovery исключаются
строки с MIB_IF_ROW2.InterfaceAndOperStatusFlags.FilterInterface: WFP/Npcap/VirtualBox и другие
NDIS filter modules не выдаются как отдельные NIC. Физические и логические интерфейсы, включая
виртуальные/VPN/loopback, остаются видимыми. Метки: ifname (Alias, UTF-8), ifindex,
guid. История привязана к GUID + NET_LUID, а не к имени или позиции перечисления.
Переименование меняет метки; изменение LUID создаёт новую серию. Повторное использование
той же идентичности драйвером нельзя безусловно обнаружить: уменьшение счётчика сбрасывает rate.
RX/TX bytes — InOctets/OutOctets, packets = unicast + non-unicast; errors — InErrors/OutErrors.
Счётчики — uint64; rate делит целочисленную разность на реальное монотонное время.
MAC — текущий PhysicalAddress, не обещание аппаратного permanent address; у некоторых интерфейсов
его нет (NO_DATA). Link speed — ReceiveLinkSpeed/TransmitLinkSpeed, сообщённые драйвером,
не измеренная пропускная способность. Нулевое значение link speed сохраняется как ответ API.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getiftable2
