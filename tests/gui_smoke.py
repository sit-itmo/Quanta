#!/usr/bin/env python3
"""Full ImGui GUI smoke test; requires an actual QUANTA_WITH_GUI=ON build and Xvfb."""
import pathlib,subprocess,sys
if len(sys.argv)!=2:raise SystemExit('usage: gui_smoke.py path/to/GUI/quanta')
query='mode gui; every 1s; for 2s; select os.uptime as plot; select os.name as text; select optional cpu.utilization as gauge(min=0,max=1);'
raise SystemExit(subprocess.call([sys.executable,str(pathlib.Path(__file__).with_name('x11_smoke.py')),sys.argv[1],'run','-q',query]))
