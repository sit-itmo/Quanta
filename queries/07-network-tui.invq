version 1;
mode tui;
every 500ms;
history 2m;
select device.net.state as text every 5s;
select rate(device.net.rx_bytes_total) as plot(unit="KiB/s", title="RX");
select rate(device.net.tx_bytes_total) as plot(unit="KiB/s", title="TX");
select delta(device.net.rx_errors_total) as value(title="RX errors per interval");
