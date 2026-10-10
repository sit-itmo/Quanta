version 1;
mode tui;
every 1s;
history 2m;
select cpu.utilization as gauge(min=0,max=1,unit="%",title="Whole system");
select optional cpu.core.utilization as bar(min=0,max=1,unit="%");
