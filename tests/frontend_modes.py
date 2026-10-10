#!/usr/bin/env python3
"""CLI entrypoints/format contracts; uses only the always-available OS provider."""
import csv
import io
from pathlib import Path
import subprocess
import sys
import tempfile

main, cli, csv_exe, txt_exe = map(str, map(Path, sys.argv[1:5]))
checks = 0

def run(exe, *args, status=0, **kwargs):
    global checks
    result = subprocess.run([exe, *args], capture_output=True, text=True,
                            timeout=20, **kwargs)
    if result.returncode != status:
        raise AssertionError((args, result.returncode, result.stdout, result.stderr))
    checks += 1
    return result.stdout

value = run(main, 'get', 'os.arch', '--format', 'value')
if len(value.splitlines()) != 1 or not value.strip() or 'os.arch' in value:
    raise AssertionError(value)
if run(cli, 'get', 'os.arch', '--format', 'value') != value:
    raise AssertionError('CLI entrypoint differs')
records = list(csv.DictReader(io.StringIO(run(csv_exe, 'get', 'os.arch'))))
if len(records) != 1 or records[0]['status'] != 'OK' or records[0]['key'] != 'os.arch':
    raise AssertionError(records)
if run(csv_exe, 'get', 'os.arch', '--format', 'value') != value:
    raise AssertionError('explicit CSV format override ignored')
table = run(txt_exe, 'get', 'os.arch')
if 'os.arch' not in table or 'OK' not in table:
    raise AssertionError(table)
with tempfile.TemporaryDirectory(prefix='quanta frontend ') as temporary:
    query = Path(temporary) / 'query with spaces.q'
    query.write_text('version 1; mode cli; once; select provider("os", os.arch);', encoding='utf-8')
    if run(main, str(query), '--format', 'value') != value:
        raise AssertionError('query shorthand/explicit provider differs')
    run(main, str(query), '--input', 'capture.qblob', status=2)
    run(main, str(query), '--record', 'capture.qblob', status=2)
run(main, 'run', '-q', 'once; select provider(os, os.arch);', status=2)
print(f'{checks} frontend entrypoint/format cases passed')
