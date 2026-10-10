#!/usr/bin/env python3
"""HAL selection and architecture boundary contracts; no privileged I/O."""
from pathlib import Path
import subprocess
import sys
exe = sys.argv[1]
root = Path(sys.argv[2])
checks = 0

def run(*args, status=0):
    global checks
    p = subprocess.run([exe, *args], text=True, capture_output=True, timeout=20)
    assert p.returncode == status, (args, p.returncode, p.stdout, p.stderr)
    checks += 1
    return p.stdout + p.stderr

names = run('--list-hals').splitlines()
assert 'none' in names and 'native' in names
assert 'Implemented operations: 0x00000000' in run('hal-info', '--hal', 'none')
assert 'Hardware writes: disabled' in run('hal-info')
assert 'HAL: none' in run('explain', '-q', 'once; hal "none"; select os.arch;')
assert 'HAL: none' not in run('explain', '-q', 'once; hal "none"; select os.arch;', '--hal', 'native')
run('get', 'os.arch', '--hal', 'nonexistent-hal', status=1)
run('hal-info', '--hal', 'nonexistent-hal', status=2)
run('get', 'os.arch', '--hal', '', status=2)
run('get', 'os.arch', '--hal', 'none', '--hal', 'native', status=2)
run('run', '-q', 'hal "none"; hal "native"; select os.arch;', status=2)
run('run', '-q', 'select provider("os", os.arch);', status=2)
run('get', 'os.arch', '--allow-hardware-write', status=2)
# Distributed examples are part of the migration contract, not historical data.
for path in (root / 'examples/queries').rglob('*.q'):
    assert 'provider(' not in path.read_text(encoding='utf-8'), path
run(str(root / 'examples/queries/00-baseline.q'), '--hal', 'none')
run(str(root / 'examples/queries/08-hal-none.q'))
# The source tree must not silently grow the removed abstraction again.
for directory in ('core', 'query', 'hal', 'osal', 'sensors'):
    for path in (root / directory).rglob('*'):
        if path.suffix not in ('.c', '.h', '.def'):
            continue
        text = path.read_text(encoding='utf-8')
        assert 'quanta_provider_desc' not in text, path
        assert 'quanta_read_ctx' not in text, path
        if directory in ('hal', 'osal'):
            assert 'quanta_smbios_parse' not in text, path
            assert 'quanta_native_dmi_refresh' not in text, path
        checks += 1
assert not (root / 'core/include/quanta/provider.h').exists()
config = (root / 'firmware/esp8266/sdkconfig.defaults').read_text()
assert '4096CONFIG' not in config
assert (root / 'firmware/esp8266/main/app_main.c').exists()
assert len(__import__('re').findall(r'\.key\s*=\s*"mcu\.', (root / 'metrics/mcu.def').read_text())) == 9
assert '0.4.0' in run('--version')
assert 'FILEVERSION 0,4,0,0' in (root / 'resources/quanta.rc').read_text()
assert 'version="0.4.0.0"' in (root / 'resources/quanta.manifest').read_text()
print(f'{checks} HAL selection/source-boundary checks passed')
