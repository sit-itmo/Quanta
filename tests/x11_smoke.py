#!/usr/bin/env python3
"""Real local Xvfb authorization/presentation test. Does not fake ImGui."""
import os,pathlib,shutil,struct,subprocess,sys,tempfile,time

def field(value):return struct.pack('>H',len(value))+value

def main():
    if len(sys.argv)<2:raise RuntimeError('usage: x11_smoke.py EXECUTABLE [ARG ...]')
    xvfb=shutil.which('Xvfb')
    if not xvfb:raise RuntimeError('Xvfb is required')
    command=[str(pathlib.Path(sys.argv[1]).resolve()),*sys.argv[2:]]
    with tempfile.TemporaryDirectory(prefix='inv-x11-') as tmp:
        tmp=pathlib.Path(tmp);p=None
        for n in range(180,240):
            if pathlib.Path('/tmp/.X11-unix/X'+str(n)).exists():continue
            auth=tmp/'Xauthority';cookie=os.urandom(16)
            auth.write_bytes(struct.pack('>H',65535)+field(b'')+field(str(n).encode())+field(b'MIT-MAGIC-COOKIE-1')+field(cookie))
            auth.chmod(0o600)
            p=subprocess.Popen([xvfb,':'+str(n),'-screen','0','1280x800x24','-auth',str(auth),'-nolisten','tcp'],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
            for _ in range(40):
                if p.poll() is not None:break
                if pathlib.Path('/tmp/.X11-unix/X'+str(n)).exists():break
                time.sleep(.05)
            if p.poll() is None and pathlib.Path('/tmp/.X11-unix/X'+str(n)).exists():break
            if p.poll() is None:p.terminate();p.wait(timeout=5)
            p=None
        if p is None:raise RuntimeError('Cannot start an isolated Xvfb display')
        try:
            env=dict(os.environ,DISPLAY=':'+str(n),XAUTHORITY=str(auth))
            result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=20)
            if result.returncode:raise RuntimeError('Authorized X11 run failed: '+result.stderr)
            print('Authorized X11 execution passed:',result.stdout.strip())
            env['XAUTHORITY']=str(tmp/'absent');env['HOME']=str(tmp)
            result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=20)
            if result.returncode==0:raise RuntimeError('Unauthorized X11 connection was unexpectedly accepted')
            print('Unauthorized X11 connection rejected as expected')
        finally:
            p.terminate();p.wait(timeout=5)
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError,subprocess.TimeoutExpired) as exc:print('x11_smoke:',exc,file=sys.stderr);sys.exit(1)
