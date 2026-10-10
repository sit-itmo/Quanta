# OS metrics use native APIs and do not require hardware access.
version 1;
mode cli;
format table;
hal "none";
once;
select os.name;
select os.arch;
select os.uptime;
