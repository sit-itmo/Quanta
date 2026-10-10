# Interface labels are matched using '*' and '?'. Edit the filter to match your host.
version 1;
mode file;
format table;
every 1s;
count 5;
select optional rate(device.net.rx_bytes_total{ifname="e*"}) as value(unit="KiB/s");
select optional rate(device.net.tx_bytes_total{ifname="e*"}) as value(unit="KiB/s");
