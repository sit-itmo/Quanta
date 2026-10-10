version 1;
mode gui;
every 1s;
select os.* as table;
select optional cpu.* as table;
select optional board.* as table;
select optional sensor.* as table every 2s;
