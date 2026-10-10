#!/usr/bin/env python3
"""Native Windows black-box tests. Never used as evidence on a Linux host."""
import csv, io, json, os, pathlib, subprocess, sys, tempfile
if os.name != 'nt':
    raise SystemExit('This suite needs a real Windows process environment.')
BIN = str(pathlib.Path(sys.argv[1]).resolve())
ROOT = pathlib.Path(__file__).resolve().parents[1]
checks = 0

def run(*args, code=0):
    global checks
    p = subprocess.run([BIN, *args], capture_output=True, timeout=35)
    stdout = p.stdout.decode('utf-8', errors='strict')
    stderr = p.stderr.decode('utf-8', errors='strict')
    assert p.returncode == code, (args, p.returncode, stdout[-1000:], stderr)
    checks += 1
    return stdout, stderr

def rows(text):
    data = list(csv.DictReader(io.StringIO(text)))
    for row in data:
        assert None not in row and all(v is not None for v in row.values()), row
        json.loads(row['labels'])
        if row['status'] != 'OK': assert row['value'] == '', row
    return data

version, _ = run('--version')
compiled, _ = run('list', '--compiled')
keys = {line.split()[0] for line in compiled.splitlines()[1:] if line.strip()}
assert {'os.name','os.arch','os.uptime'} <= keys
assert 'os.swap.used_bytes' not in keys, 'Windows commit must not masquerade as Linux swap'
assert 'os.load.1m' not in keys
out, _ = run('get','os.name','os.arch','os.kernel.release','--format','csv')
assert len(rows(out)) == 3 and all(x['status']=='OK' for x in rows(out))
for query in ('select nonexistent.metric;', 'select os.name as plot;', 'select rate(os.uptime);',
              'mode gui; mode tui; select os.name;', 'once; count 2; select os.name;'):
    run('run','-q',query,code=2)
run('run','-q','once; select optional missing.future.metric;')
run('run','-q','strict; every 100ms; select os.uptime;',code=2)
run('describe','os.name')
if 'GUI=0' in version: run('run','-q','mode gui; select os.name;',code=2)
if 'TUI=0' in version: run('run','-q','mode tui; select os.name;',code=2)
if 'os.memory.used_bytes' in keys:
    out, _ = run('get','os.memory.*','--format','csv')
    data = {r['key']:r for r in rows(out)}
    total, avail, used = [int(data['os.memory.'+key]['value']) for key in ('total_bytes','available_bytes','used_bytes')]
    assert total > 0 and total - avail == used
    assert 0 <= float(data['os.memory.utilization']['value']) <= 1
    run('get','windows.memory.commit.*','--format','table')
if 'cpu.utilization' in keys:
    out, _ = run('run','-q','every 250ms; count 5; format csv; select cpu.utilization;')
    data = rows(out)
    assert data
    # PDH may be unavailable on the runner; that is not equivalent to 0% CPU.
    for r in data:
        if r['status']=='OK': assert 0 <= float(r['value']) <= 1
    run('get','cpu.utilization','--warmup','1200ms','--format','csv')
if 'device.net.rx_bytes_total' in keys:
    out, _ = run('run','-q','every 250ms; count 4; format csv; select optional rate(device.net.rx_bytes_total);')
    for r in rows(out):
        if r['status']=='OK': assert float(r['value']) >= 0 and int(r['window_ns']) > 0
with tempfile.TemporaryDirectory(prefix='quanta-windows-') as folder:
    root = pathlib.Path(folder)
    query = root / '\u0437\u0430\u043f\u0440\u043e\u0441.q'
    output = root / '\u043e\u0442\u0447\u0451\u0442.csv'
    query.write_text('version 1; once; mode file; format csv; select os.name;',encoding='utf-8-sig')
    run('run','--query-file',str(query),'--output',str(output))
    before = output.read_bytes(); assert b'\r\r\n' not in before
    assert len(rows(before.decode('utf-8'))) == 1
    run('get','os.name','--format','csv','--output',str(output),code=1)
    assert output.read_bytes()==before
    query.write_bytes(b'select os.name;\0')
    run('run','--query-file',str(query),code=2)
    query.write_bytes(b' '*70000)
    run('run','--query-file',str(query),code=2)
# Complete example plans validate in a full build. GUI/TUI are switched to FILE
# for this test ONLY; this does not test those frontends.
if {'windows.memory.commit.used_bytes','cpu.utilization','process.self.pid','fs.root.total_bytes'} <= keys:
    queries = sorted((ROOT/'examples/queries/windows').glob('*.q'))
    assert queries, 'Windows example directory is missing'
    for query in queries:
        run('explain','--query-file',str(query),'--mode','file')
        args = ['run','--query-file',str(query),'--mode','file']
        if 'once;' not in query.read_text(encoding='utf-8'): args += ['--count','2','--duration','250ms']
        run(*args)
print(f'{checks} native Windows CLI cases passed')
