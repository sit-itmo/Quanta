# Windows exports thermal zones only if firmware/driver counters are available.
# These are NOT automatically CPU package/core temperatures. No driver is installed.
version 1;
mode file;
format table;
every 2s;
count 5;
select optional sensor.temperature;
