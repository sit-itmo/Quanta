#!/usr/bin/env python3
"""Black-box tests. No assumptions about serial numbers or sensors on the host."""
import csv, io, json, os, pathlib, subprocess, sys, tempfile
BIN = str(pathlib.Path(sys.argv[1]).resolve())
checks = 0
def run(*args, code=0, **kw):
    global checks
    p = subprocess.run([BIN, *args], text=True, capture_output=True, timeout=20, **kw)
    assert p.returncode == code, (args, p.returncode, p.stdout[-1000:], p.stderr)
    checks += 1
    return p

def rows(text):
    out = list(csv.DictReader(io.StringIO(text)))
    for row in out:
        assert None not in row and all(v is not None for v in row.values()), row
        json.loads(row['labels'])
    return out

version = run('--version').stdout
compiled = run('list', '--compiled').stdout
keys = {line.split()[0] for line in compiled.splitlines()[1:] if line.strip()}
assert 'os.name' in keys and 'os.uptime' in keys
run('get', 'os.name', 'os.kernel.release', '--format', 'human')
p = run('get', 'os.name', 'os.arch', '--format', 'csv')
r = rows(p.stdout)
assert len(r) == 2 and all(x['status'] == 'OK' for x in r)
run('run', '-q', 'select not.a.metric;', code=2)
run('run', '-q', 'mode file; mode gui; select os.name;', code=2)
run('run', '-q', 'select os.name as plot;', code=2)
run('run', '-q', 'select rate(os.uptime);', code=2)
run('run', '-q', 'select os.name as value(title="x\\q");', code=2)
run('run', '-q', 'once; count 2; select os.name;', code=2)
run('get', 'os.name', '--count', '-1', code=2)
run('get', 'os.name', '--every', 'nan', code=2)
run('run', '--query', 'select os.name;', '--query-file', 'other', code=2)
p = run('run', '-q', 'once; select optional missing.future.metric;')
assert 'optional' in p.stderr and not 'ERROR' in p.stdout
p = run('run', '-q', 'mode file; format csv; every 100ms; count 3; select os.uptime;')
r = rows(p.stdout)
assert r and r[0]['status'] == 'OK'
assert 'clamped' in p.stderr
run('run', '-q', 'mode file; strict; every 100ms; select os.uptime;', code=2)
run('get', 'os.name', '--format', 'md')
run('describe', 'os.name')
if 'TUI=0' in version:
    run('run', '-q', 'mode tui; select os.name;', code=2)
if 'GUI=0' in version:
    run('run', '-q', 'mode gui; select os.name;', code=2)
if 'os.memory.utilization' in keys:
    p = run('get', 'os.memory.*', '--format', 'csv')
    data = {x['key']: x for x in rows(p.stdout)}
    total, avail, used = (int(data['os.memory.'+name]['value']) for name in ['total_bytes','available_bytes','used_bytes'])
    assert total - avail == used and 0 <= float(data['os.memory.utilization']['value']) <= 1
    p = run('explain', '-q', 'select os.memory.used_bytes; select os.memory.used_bytes as plot;')
    assert 'Acquisition channels: 1; output series/views: 2' in p.stdout
if 'cpu.utilization' in keys:
    p = run('run', '-q', 'mode file; format csv; every 100ms; count 4; select cpu.utilization;')
    data = rows(p.stdout)
    assert data[0]['status'] == 'WARMUP' and data[0]['value'] == ''
    for row in data:
        if row['status'] == 'OK': assert 0 <= float(row['value']) <= 1
    p = run('get', 'cpu.utilization', '--format', 'csv', '--warmup', '1200ms')
    assert len(rows(p.stdout)) == 1
if 'device.net.rx_bytes_total' in keys:
    p = run('run', '-q', 'mode file; format csv; every 100ms; count 3; select optional rate(device.net.rx_bytes_total);')
    data = rows(p.stdout)
    for row in data:
        if row['status'] == 'OK': assert float(row['value']) >= 0 and int(row['window_ns']) > 0
with tempfile.TemporaryDirectory(prefix='inv-integration-') as tmp:
    path = pathlib.Path(tmp)/'result.csv'
    run('get', 'os.name', '--format', 'csv', '--output', str(path))
    assert rows(path.read_text())
    before = path.read_bytes()
    run('get', 'os.name', '--format', 'csv', '--output', str(path), code=1)
    assert path.read_bytes() == before
    q = pathlib.Path(tmp)/'query.invq'
    q.write_text('mode file; format csv; once; select os.name;')
    assert len(rows(run('run', '--query-file', str(q)).stdout)) == 1
    q.write_bytes(b'select os.name;\x00select os.hostname;')
    run('run', '--query-file', str(q), code=2)
    q.write_text(' ' * 70000)
    run('run', '--query-file', str(q), code=2)
print(f'{checks} CLI integration cases passed')
