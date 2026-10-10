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
    assert not re.search(r'#\s*include\s*[<"](?:windows|pthread|unistd|sys/)', path.read_text(encoding="utf-8")), path
    checks += 1
for area in ("core", "hal", "osal", "sensors"):
    for path in (root / area).rglob("*"):
        if path.suffix not in (".c", ".h", ".def"):
            continue
        text = path.read_text(encoding="utf-8")
        assert "quanta_provider_desc" not in text and "quanta_read_ctx" not in text, path
        if area in ("hal", "osal"):
            assert "quanta_smbios_parse" not in text and "quanta_native_dmi_refresh" not in text, path
for path in (root / "examples/queries").rglob("*.q"):
    assert "provider(" not in path.read_text(encoding="utf-8"), path
assert (root / "sensors/platform/esp8266/mcu.c").is_file()
assert (root / "firmware/esp8266/main/app_main.c").is_file()
assert len(re.findall(r'QUANTA_META(?:_DESC|_VERSION)?\(\s*"mcu\.', (root / "metrics/mcu.def").read_text(encoding="utf-8"))) == 9
assert "4096CONFIG" not in (root / "firmware/esp8266/sdkconfig.defaults").read_text(encoding="utf-8")
version = re.search(r'#define QUANTA_VERSION "([^"]+)"', (root / "include/quanta/types.h").read_text(encoding="utf-8")).group(1)
assert "FILEVERSION " + version.replace(".", ",") + ",0" in (root / "resources/quanta.rc").read_text(encoding="utf-8")
assert 'version="' + version + '.0"' in (root / "resources/quanta.manifest").read_text(encoding="utf-8")
assert (root / "vendor/imgui/imgui.cpp").is_file()
assert '"1.92.9b"' in (root / "vendor/imgui/imgui.h").read_text(encoding="utf-8")
# Keep the supplied upstream subset intact; the adapter lives outside vendor/.
manifest = (root / "vendor/IMGUI.md").read_text(encoding="utf-8")
upstream = dict((name, digest) for digest, name in re.findall(r"^([0-9a-f]{64})  (.+)$", manifest, re.M))
assert upstream
for name, digest in upstream.items():
    assert hashlib.sha256((root / "vendor/imgui" / name).read_bytes()).hexdigest() == digest, name
assert {p.relative_to(root / "vendor/imgui").as_posix() for p in (root / "vendor/imgui").rglob("*") if p.is_file()} == set(upstream)
# The complete supplied GLFW tree is immutable, including its original license.
glfw_manifest = (root / "vendor/GLFW.sha256").read_text(encoding="ascii")
glfw_files = dict((name, digest) for digest, name in
                  re.findall(r"^([0-9a-f]{64})  (.+)$", glfw_manifest, re.M))
assert len(glfw_files) == 167, "incomplete GLFW 3.4 source manifest"
for name, digest in glfw_files.items():
    assert hashlib.sha256((root / "vendor/glfw" / name).read_bytes()).hexdigest() == digest, name
assert {p.relative_to(root / "vendor/glfw").as_posix()
        for p in (root / "vendor/glfw").rglob("*") if p.is_file()} == set(glfw_files)
glfw_header = (root / "vendor/glfw/include/GLFW/glfw3.h").read_text(encoding="utf-8")
assert re.search(r"#define\s+GLFW_VERSION_MAJOR\s+3\b", glfw_header)
assert re.search(r"#define\s+GLFW_VERSION_MINOR\s+4\b", glfw_header)
for removed in ("fetch_imgui.py", "build.sh", "build_windows.cmd", "build_windows.ps1"):
    assert not (root / "tools" / removed).exists(), removed
for path in (root / "frontends").rglob("*.cpp"):
    assert "std::quanta_" not in path.read_text(encoding="utf-8"), path
# All first-party source headers describe the file and explain local concepts.
source_headers = 0
for path in root.rglob("*"):
    relative = path.relative_to(root)
    if not path.is_file() or path.suffix not in (".c", ".cpp", ".h", ".def"):
        continue
    # Do not examine arbitrary in-tree build directories or upstream sources.
    if relative.parts[0] not in ("app", "core", "hal", "osal", "sensors", "frontends",
                                  "include", "metrics", "firmware", "tests"):
        continue
    if "build" in relative.parts or "reference" in relative.parts:
        continue
    header = path.read_text(encoding="utf-8").split("*/", 1)[0]
    assert "@file " + relative.as_posix() in header, path
    assert len(header.splitlines()) >= 7, path
    source_headers += 1
assert source_headers >= 175
cmake_ui = (root / "cmake/Frontends.cmake").read_text(encoding="utf-8")
assert 'set(QUANTA_GUI_BACKEND "OPENGL" CACHE STRING' in cmake_ui
assert 'add_subdirectory("${QUANTA_GLFW_DIR}"' in (root / "frontends/CMakeLists.txt").read_text(encoding="utf-8")
assert "/vendor/imgui/*" not in (root / ".gitignore").read_text(encoding="utf-8")
for path in [root / "CMakeLists.txt", *(root / "cmake").glob("*.cmake"),
             root / "frontends/CMakeLists.txt"]:
    assert not re.search(r"FetchContent_|ExternalProject_Add|file\(DOWNLOAD", path.read_text(encoding="utf-8")), path
# First-party code and build scripts use English ASCII text. Unicode test data
# uses escapes; embedded Markdown remains Russian UTF-8 at runtime. This catches
# accidental non-English text, not every possible natural-language mistake.
code_extensions = {".c", ".cpp", ".h", ".def", ".py", ".cmake", ".sh", ".ps1",
                   ".cmd", ".q", ".rc", ".in", ".yml"}
code_roots = {"app", "core", "hal", "osal", "sensors", "frontends", "include",
              "metrics", "firmware", "tests", "tools", "cmake", "examples",
              "resources", ".github"}
language_files = 0
for path in root.rglob("*"):
    relative = path.relative_to(root)
    if not path.is_file() or "reference" in relative.parts or "build" in relative.parts:
        continue
    if relative.parts[0] not in code_roots and relative.as_posix() != "CMakeLists.txt":
        continue
    if path.suffix not in code_extensions and path.name != "CMakeLists.txt":
        continue
    text = path.read_text(encoding="utf-8")
    assert text.isascii(), (relative, "first-party source must use English text; escape Unicode data")
    language_files += 1
for directory in (root / "docs",):
    for path in directory.rglob("*.md"):
        assert re.search(r"[\u0400-\u04ff]", path.read_text(encoding="utf-8")), path
print(f"Source contract: {checks} portable/public files, {source_headers} source headers, "
      f"{language_files} English-source files, {len(glfw_files)} GLFW originals; "
      "layout/MCU/vendor/language checks passed")
