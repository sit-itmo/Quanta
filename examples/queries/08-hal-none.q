# Native OS metrics do not need hardware primitives.
version 1;
hal "none";
once;
select os.name;
select os.arch;
