#!/usr/bin/env python3
"""Architecture regression checks, not a C parser or a formal dependency proof."""
from pathlib import Path
import re
import hashlib
import sys

root = Path(sys.argv[1]).resolve()
assert (root / "core/src/query.c").is_file()
assert not (root / "query").exists()
assert (root / "include/quanta/osal.h").is_file()
for area in ("core", "hal", "osal", "sensors", "frontends"):
    assert not (root / area / "include").exists(), area

# Strip comments and literals before looking for calls (help text is not code).
ignore = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
plain_calls = (
    "malloc calloc realloc free memset memcpy memmove memcmp memchr strlen strcmp strncmp "
    "strchr strrchr strstr strcpy strtol strtoll strtoull strtod snprintf vsnprintf printf "
    "fprintf puts fputs fputc fgetc fgets fread fwrite fseek ftell fopen fclose fflush ferror "
    "feof clearerr strerror isfinite floor ceil pow fabs isspace isdigit isalpha isalnum "
    "open close read write mmap socket getpid gmtime_r gmtime_s strftime setlocale"
).split()
call = re.compile(r'(?<![\w.>])(?:' + '|'.join(plain_calls) + r')\s*\(')
checks = 0
for area in ("core", "frontends/common", "hal/common", "sensors/common", "frontends/dispatch.c", "app/main.c"):
    path = root / area
    paths = [path] if path.is_file() else path.rglob("*")
    for path in paths:
        if path.suffix not in (".c", ".h"):
            continue
        text = path.read_text(encoding="utf-8")
        includes = re.findall(r'#\s*include\s*[<"]([^>"]+)', text)
        for header in includes:
            assert not re.match(r'(windows|pthread|unistd|signal|fcntl|dirent|termios|sys/|xcb/)', header), (path, header)
        code = ignore.sub(" ", text)
        assert not call.search(code), (path, call.search(code).group(0) if call.search(code) else "")
        checks += 1
for path in (root / "include/quanta").glob("*.h"):
    assert not re.search(r'#\s*include\s*[<"](?:windows|pthread|unistd|sys/)', path.read_text()), path
    checks += 1
for area in ("core", "hal", "osal", "sensors"):
    for path in (root / area).rglob("*"):
        if path.suffix not in (".c", ".h", ".def"):
            continue
        text = path.read_text()
        assert "quanta_provider_desc" not in text and "quanta_read_ctx" not in text, path
        if area in ("hal", "osal"):
            assert "quanta_smbios_parse" not in text and "quanta_native_dmi_refresh" not in text, path
for path in (root / "examples/queries").rglob("*.q"):
    assert "provider(" not in path.read_text(), path
assert (root / "sensors/platform/esp8266/mcu.c").is_file()
assert (root / "firmware/esp8266/main/app_main.c").is_file()
assert len(re.findall(r'QUANTA_META(?:_DESC|_VERSION)?\(\s*"mcu\.', (root / "metrics/mcu.def").read_text())) == 9
assert "4096CONFIG" not in (root / "firmware/esp8266/sdkconfig.defaults").read_text()
version = re.search(r'#define QUANTA_VERSION "([^"]+)"', (root / "include/quanta/types.h").read_text()).group(1)
assert "FILEVERSION " + version.replace(".", ",") + ",0" in (root / "resources/quanta.rc").read_text()
assert 'version="' + version + '.0"' in (root / "resources/quanta.manifest").read_text()
assert (root / "vendor/imgui/imgui.cpp").is_file()
assert '"1.92.9b"' in (root / "vendor/imgui/imgui.h").read_text()
# Keep the supplied upstream subset intact; the adapter lives outside vendor/.
manifest = (root / "vendor/IMGUI.md").read_text()
upstream = dict((name, digest) for digest, name in re.findall(r"^([0-9a-f]{64})  (.+)$", manifest, re.M))
assert upstream
for name, digest in upstream.items():
    assert hashlib.sha256((root / "vendor/imgui" / name).read_bytes()).hexdigest() == digest, name
assert {p.relative_to(root / "vendor/imgui").as_posix() for p in (root / "vendor/imgui").rglob("*") if p.is_file()} == set(upstream)
for removed in ("fetch_imgui.py", "build.sh", "build_windows.cmd", "build_windows.ps1"):
    assert not (root / "tools" / removed).exists(), removed
for path in (root / "frontends").rglob("*.cpp"):
    assert "std::quanta_" not in path.read_text(), path
print(f"Source contract: {checks} portable/public source files plus layout and MCU checks passed")
