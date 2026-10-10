#!/usr/bin/env python3
"""Fetch the exact optional ImGui sources; never used by the running program.
HTTPS plus immutable commit pin. Generated hashes are provenance, not an external
trusted checksum. Prefer an independently audited offline source tree when needed.
"""
import argparse,hashlib,json,pathlib,shutil,sys,tempfile,urllib.request
ROOT=pathlib.Path(__file__).resolve().parents[1]
COMMIT='f5befd2d29e66809cd1110a152e375a7f1981f06'
FILES=('imgui.h','imconfig.h','imgui_internal.h','imstb_rectpack.h',
       'imstb_textedit.h','imstb_truetype.h','imgui.cpp','imgui_draw.cpp',
       'imgui_tables.cpp','imgui_widgets.cpp','LICENSE.txt')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--destination',type=pathlib.Path,default=ROOT/'vendor/imgui')
    args=ap.parse_args();dst=args.destination.resolve()
    if any((dst/f).exists() for f in FILES):
        raise RuntimeError('Refusing to replace existing ImGui source files: '+str(dst))
    dst.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.quanta-imgui-',dir=dst.parent) as stage:
        stage=pathlib.Path(stage);hashes={}
        for name in FILES:
            url='https://raw.githubusercontent.com/ocornut/imgui/'+COMMIT+'/'+name
            print('Fetching',name,flush=True)
            request=urllib.request.Request(url,headers={'User-Agent':'quanta-source-fetch/0.1.0'})
            with urllib.request.urlopen(request,timeout=60) as response:
                data=response.read(8*1024*1024+1)
            if not data or len(data)>8*1024*1024: raise RuntimeError('Unexpected source size: '+name)
            (stage/name).write_bytes(data);hashes[name]=hashlib.sha256(data).hexdigest()
        if '#define IMGUI_VERSION       "1.91.9b"' not in (stage/'imgui.h').read_text():
            # Be whitespace-tolerant while still checking the exact release string.
            import re
            if not re.search(r'#define\s+IMGUI_VERSION\s+"1\.91\.9b"', (stage/'imgui.h').read_text()):
                raise RuntimeError('The downloaded ImGui header does not match v1.91.9b')
        (stage/'provenance.json').write_text(json.dumps({'upstream':'ocornut/imgui','commit':COMMIT,'sha256':hashes},indent=2)+'\n')
        dst.mkdir(parents=True,exist_ok=True)
        for source in stage.iterdir():shutil.move(str(source),str(dst/source.name))
    print('Source dependency ready:',dst)
    print('Configure with -DQUANTA_WITH_GUI=ON -DQUANTA_FETCH_IMGUI=OFF')
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError) as exc:print('fetch_imgui:',exc,file=sys.stderr);sys.exit(1)
