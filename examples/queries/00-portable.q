# Works without native OS metrics, a terminal, a window or privileged hardware.
version 1;
mode cli;
hal "none";
once;
select quanta.version;
select hal.name;
select hal.capabilities;
