#!/usr/bin/env python3
"""Native ImGui/Win32 smoke: actual window, WM_SIZE/WM_CHAR/WM_CLOSE and clean exit.
This is not a screenshot comparison or a substitute for manual UI testing.
"""
import ctypes, ctypes.wintypes as wt, os, pathlib, subprocess, sys, time
if os.name != 'nt': raise SystemExit('Requires Windows and a real QUANTA_WITH_GUI=ON build.')
user32 = ctypes.WinDLL('user32', use_last_error=True)
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetWindowThreadProcessId.restype = wt.DWORD
user32.PostMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user32.PostMessageW.restype = wt.BOOL
user32.SetWindowPos.argtypes = [wt.HWND, wt.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wt.UINT]
user32.SetWindowPos.restype = wt.BOOL
ENUM = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.EnumWindows.argtypes = [ENUM, wt.LPARAM]
user32.EnumWindows.restype = wt.BOOL
binary = str(pathlib.Path(sys.argv[1]).resolve())
query = 'mode gui; every 1s; for 15s; select os.uptime as plot; select os.name as text; select optional cpu.utilization as gauge(min=0,max=1);'
p = subprocess.Popen([binary,'run','-q',query],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
try:
    found = []
    @ENUM
    def visit(hwnd, _):
        pid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
        if pid.value == p.pid: found.append(hwnd); return False
        return True
    limit = time.monotonic()+10
    while time.monotonic()<limit and p.poll() is None:
        user32.EnumWindows(visit,0)
        if found: break
        time.sleep(.05)
    assert found, ('Quanta window not found',p.poll())
    hwnd = found[0]
    time.sleep(.5)
    assert user32.SetWindowPos(hwnd,None,0,0,960,700,0x0002|0x0004)  # NOMOVE|NOZORDER
    # Send valid UTF-16 code units; without a focused text field these must be harmless.
    for cp in (0x0442,0x0435,0x0441,0x0442): assert user32.PostMessageW(hwnd,0x0102,cp,0)
    time.sleep(.5)
    assert user32.PostMessageW(hwnd,0x0010,0,0)  # WM_CLOSE
    out,err = p.communicate(timeout=15)
    assert p.returncode==0,(p.returncode,out.decode('utf-8','replace'),err.decode('utf-8','replace'))
    print('Native GUI: window created, resized, Unicode events processed, clean WM_CLOSE exit')
finally:
    if p.poll() is None: p.kill();p.communicate()
