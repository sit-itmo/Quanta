#!/usr/bin/env python3
"""Run the actual ImGui application on a private Xvfb; not a visual acceptance test."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import time

if len(sys.argv) != 2:
    raise SystemExit('usage: gui_smoke.py path/to/quanta')
exe = str(Path(sys.argv[1]).resolve())
query = ('mode gui; every 100ms; for 2s; '
         'select quanta.version as text; select os.uptime as plot; '
         'select optional cpu.utilization as gauge(min=0,max=1);')
# -displayfd asks Xvfb to allocate a free display rather than racing on :99.
with tempfile.TemporaryFile(mode='w+b') as number, tempfile.TemporaryFile(mode='w+b') as log:
    server = subprocess.Popen(['Xvfb', '-displayfd', str(number.fileno()),
                               '-screen', '0', '1280x800x24', '-nolisten', 'tcp', '-ac'],
                              pass_fds=(number.fileno(),), stdout=log, stderr=log)
    try:
        display = ''
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and server.poll() is None:
            number.seek(0)
            display = number.read().decode('ascii').strip()
            if display.isdigit():
                break
            time.sleep(.05)
        if not display.isdigit():
            log.seek(0)
            raise RuntimeError('Xvfb failed to start: ' + log.read().decode('utf-8', 'replace'))
        environment = dict(os.environ, DISPLAY=':' + display, XAUTHORITY='/nonexistent/quanta-test-authority')
        result = subprocess.run([exe, 'run', '-q', query], env=environment,
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
        bad_font = subprocess.run([exe, 'run', '-q', query, '--font',
                                   '/nonexistent/quanta-test-font.ttf'], env=environment,
                                  capture_output=True, text=True, timeout=20)
        assert bad_font.returncode == 1, (bad_font.returncode, bad_font.stderr)
        assert 'cannot read GUI font' in bad_font.stderr, bad_font.stderr
        no_display = dict(environment, DISPLAY='')
        unavailable = subprocess.run([exe, 'run', '-q', query], env=no_display,
                                     capture_output=True, text=True, timeout=20)
        assert unavailable.returncode == 1, (unavailable.returncode, unavailable.stderr)
        assert unavailable.stderr.strip(), 'missing GUI dependency must be diagnosed'
        for failure in (bad_font, unavailable):
            assert 'AddressSanitizer' not in failure.stderr, failure.stderr
            assert 'runtime error:' not in failure.stderr, failure.stderr
        print('GUI: actual window/deadline, missing-font cleanup and absent-display error passed')
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
