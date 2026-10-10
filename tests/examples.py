#!/usr/bin/env python3
"""Execute all shipped queries through the FILE sink, with short durations."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];binary=str(Path(sys.argv[1]).resolve())
for q in sorted((root/'queries').glob('*.invq')):
    once='once;' in q.read_text()
    args=[binary,'run','--query-file',str(q),'--mode','file']
    if not once:args += ['--count','2','--duration','250ms']
    p=subprocess.run(args,capture_output=True,text=True,timeout=10)
    assert p.returncode==0,(q.name,p.returncode,p.stderr)
    assert p.stdout or 'optional' in p.stderr,q.name
    print(q.name,'FILE execution passed')
