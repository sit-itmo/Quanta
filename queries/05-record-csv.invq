# CSV to stdout. --output creates a new file and never overwrites an existing file.
version 1;
mode file;
format csv;
every 1s;
for 30s;
select cpu.utilization;
select os.memory.used_bytes;
select os.memory.utilization;
select optional rate(device.net.rx_bytes_total);
select optional rate(device.net.tx_bytes_total);
select optional sensor.temperature every 2s;
