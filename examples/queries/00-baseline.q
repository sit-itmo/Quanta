# Always-available OS provider; ordinary user, one shot.
version 1;
mode cli;
format table;
once;
select provider("os", os.name);
select os.arch;
select os.uptime;
