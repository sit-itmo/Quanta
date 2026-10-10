#!/usr/bin/env python3
"""Inspect a PE import table without dumpbin, objdump or executing the binary.
This checks direct imports (including delay imports), not transitive imports
inside Windows system DLLs. It is a dependency audit, NOT a compatibility proof.
"""
import argparse, pathlib, struct, sys
SYSTEM_DLLS = {'kernel32.dll','kernelbase.dll','ntdll.dll','user32.dll','gdi32.dll','advapi32.dll',
              'pdh.dll','psapi.dll','iphlpapi.dll','imm32.dll','ws2_32.dll','version.dll',
              'shell32.dll','shlwapi.dll','ole32.dll','oleaut32.dll','combase.dll','bcrypt.dll',
              'crypt32.dll','secur32.dll','userenv.dll','winmm.dll'}

def imports(path):
    data=pathlib.Path(path).read_bytes()
    def unpack(fmt, offset):
        size=struct.calcsize(fmt)
        if offset<0 or offset+size>len(data): raise ValueError('Truncated PE structure')
        return struct.unpack_from(fmt,data,offset)
    if data[:2]!=b'MZ': raise ValueError('Not a DOS/PE executable')
    pe,=unpack('<I',0x3c)
    if data[pe:pe+4]!=b'PE\0\0': raise ValueError('Missing PE signature')
    machine, sections, _, _, _, opt_size, _=unpack('<HHIIIHH',pe+4)
    if sections>4096: raise ValueError('Excessive section count')
    opt=pe+24; magic,=unpack('<H',opt)
    if magic==0x10b: directories=opt+96; imagebase,=unpack('<I',opt+28)
    elif magic==0x20b: directories=opt+112; imagebase,=unpack('<Q',opt+24)
    else: raise ValueError('Unsupported optional header')
    n_dir,=unpack('<I',directories-4)
    if directories+8*min(n_dir,16)>opt+opt_size: raise ValueError('Directory exceeds optional header')
    header_size,=unpack('<I',opt+60)
    ranges=[]
    for index in range(sections):
        section=opt+opt_size+40*index
        _,vsize,rva,raw_size,raw,_,_,_,_,_=unpack('<8sIIIIIIHHI',section)
        ranges.append((rva,max(vsize,raw_size),raw,raw_size))
    def resolve(rva, size=1):
        if rva<header_size and rva+size<=min(header_size,len(data)): return rva
        for start,span,raw,raw_size in ranges:
            if start<=rva<start+span and rva-start+size<=raw_size and raw+rva-start+size<=len(data): return raw+rva-start
        raise ValueError('RVA outside backed sections')
    def name(rva):
        chars=[]
        for index in range(512):
            byte=data[resolve(rva+index)]
            if byte==0: return bytes(chars).decode('ascii').lower()
            chars.append(byte)
        raise ValueError('Import name too long')
    def directory(index):
        return unpack('<II',directories+index*8) if index<n_dir else (0,0)
    found=set()
    for index,stride,field in [(1,20,3),(13,32,1)]:
        rva,size=directory(index)
        if not rva: continue
        if size<stride: raise ValueError('Invalid import directory size')
        terminated=False
        for offset in range(0,min(size,4096*stride),stride):
            entry=unpack('<'+'I'*(stride//4),resolve(rva+offset,stride))
            if not any(entry): terminated=True;break
            nrva=entry[field]
            if index==13 and not entry[0]&1:
                if nrva<imagebase: raise ValueError('Invalid delay import VA')
                nrva-=imagebase
            found.add(name(nrva))
        if not terminated: raise ValueError('Unterminated/excessive import directory')
    return machine, sorted(found)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe');ap.add_argument('--profile',choices=['msvc','mingw'],default='msvc')
    args=ap.parse_args()
    machine, names=imports(args.exe)
    allowed=set(SYSTEM_DLLS)
    if args.profile=='mingw': allowed.update({'msvcrt.dll','ucrtbase.dll'})
    bad=[]
    for name in names:
        system=name in allowed or name.startswith(('api-ms-win-core-','ext-ms-win-'))
        if args.profile=='mingw' and name.startswith('api-ms-win-crt-'):system=True
        if not system:bad.append(name)
    print(f'PE machine=0x{machine:04x}; profile={args.profile}; direct DLL imports:')
    for name in names:print('  '+name)
    if bad:
        print('Unexpected/non-static application dependencies: '+', '.join(bad),file=sys.stderr);return 1
    print('No unexpected application runtime DLLs in the direct/delay import tables.')
    print('Windows system DLLs remain required; this is not a no-DLL executable.')
    return 0
if __name__=='__main__':
    try:raise SystemExit(main())
    except (OSError,ValueError,struct.error) as e:print('verify_windows_imports:',e,file=sys.stderr);raise SystemExit(2)
