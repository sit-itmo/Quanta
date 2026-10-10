#!/usr/bin/env python3
"""Exercise a real pseudoterminal, including pause, filtering and restoration."""
import errno, fcntl, os, pathlib, pty, select, signal, struct, subprocess, sys, termios, time
binary = str(pathlib.Path(sys.argv[1]).resolve())
master, slave = pty.openpty()
fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 110, 0, 0))
before = termios.tcgetattr(slave)
env = dict(os.environ, TERM='xterm-256color')
p = subprocess.Popen([binary, 'run', '-q', 'mode tui; every 1s; select os.uptime as plot; select os.name;'], stdin=slave, stdout=slave, stderr=slave, env=env)
out = bytearray()
def drain(seconds):
    end = time.monotonic()+seconds
    while time.monotonic()<end:
        ready, _, _ = select.select([master], [], [], 0.05)
        if ready:
            try: out.extend(os.read(master, 65536))
            except OSError as exc:
                if exc.errno != errno.EIO: raise
                return
try:
    drain(0.5)
    os.write(master, b'p'); drain(0.3)
    os.write(master, b'p/os.name\r'); drain(0.3)
    os.write(master, b'\x1bjksr?'); drain(0.3)
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 12, 48, 0, 0))
    os.kill(p.pid, signal.SIGWINCH); drain(0.2)
    os.write(master, b'q'); drain(0.3)
    assert p.wait(timeout=5) == 0, out[-3000:]
    after = termios.tcgetattr(slave)
    assert before == after, (before,after)
    assert b'\x1b[?1049h' in out and b'\x1b[?1049l' in out and b'\x1b[?25h' in out
    assert b'DISPLAY PAUSED' in out and b'LIVE' in out
    print('TUI PTY: input, pause/resume, filter, scroll, resize and terminal restoration passed')
finally:
    if p.poll() is None: p.kill(); p.wait()
    os.close(master); os.close(slave)

master, slave = pty.openpty()
before = termios.tcgetattr(slave)
p = subprocess.Popen([binary, 'run', '-q', 'mode tui; select os.name;'], stdin=slave, stdout=slave, stderr=slave, env=env)
try:
    time.sleep(0.3)
    p.send_signal(signal.SIGTERM)
    assert p.wait(timeout=5) == 128+signal.SIGTERM
    assert before == termios.tcgetattr(slave)
    print('TUI PTY: SIGTERM cleanup and exit status passed')
finally:
    if p.poll() is None: p.kill(); p.wait()
    os.close(master); os.close(slave)
