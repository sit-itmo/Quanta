#!/usr/bin/env python3
"""Single-executable CLI, format selection and HAL contracts; no privileged I/O."""
import csv
import io
import re
from pathlib import Path
import subprocess
import sys
import tempfile

EXE = str(Path(sys.argv[1]).resolve())
ROOT = Path(sys.argv[2]).resolve()
checks = 0


def run(*args, status=0, **kwargs):
    global checks
    result = subprocess.run([EXE, *args], capture_output=True, text=True,
                            encoding="utf-8", errors="strict", timeout=20, **kwargs)
    if result.returncode != status:
        raise AssertionError((args, result.returncode, result.stdout, result.stderr))
    checks += 1
    return result


expected_version = re.search(r'#define QUANTA_VERSION "([^"]+)"',
                             (ROOT / "include/quanta/types.h").read_text(encoding="utf-8")).group(1)
version = run("--version").stdout
assert "quanta " + expected_version in version
modes = run("--list-modes").stdout.splitlines()
assert modes[0] == "file" and len(modes) == len(set(modes))
assert ("tui" in modes) == ("TUI=1" in version)
assert ("gui" in modes) == ("GUI=1" in version)
run("--list-modes", "ignored", status=2)
value = run("get", "quanta.version", "--format", "value").stdout
assert value.strip() == expected_version and len(value.splitlines()) == 1
records = list(csv.DictReader(io.StringIO(run("get", "quanta.version", "--format", "csv").stdout)))
assert len(records) == 1 and records[0]["status"] == "OK" and records[0]["key"] == "quanta.version"
for fmt in ("table", "txt", "md", "human"):
    text = run("get", "quanta.version", "--format", fmt).stdout
    assert "quanta.version" in text and expected_version in text and "OK" in text
assert run("get", "quanta.version", "--mode", "cli", "--format", "value").stdout == value
assert run("get", "quanta.version", "--cli", "--format", "value").stdout == value
run("get", "quanta.version", "--gui", "--cli", status=2)
run("get", "quanta.version", "--mode", "file", "--cli", status=2)
run("get", "quanta.version", "--cli", "--mode", "file", status=2)
run("get", "quanta.version", "--mode", "unknown", status=2)
for mode in ("gui", "tui"):
    if mode not in modes:
        result = run("get", "quanta.version", "--" + mode, status=2)
        assert "not compiled" in result.stderr

names = run("--list-hals").stdout.splitlines()
assert "none" in names and "native" in names
assert "Implemented operations: 0x00000000" in run("hal-info", "--hal", "none").stdout
assert "Hardware writes: disabled" in run("hal-info").stdout
assert "HAL: none" in run("explain", "-q", 'once; hal "none"; select quanta.version;').stdout
assert "HAL: none" not in run("explain", "-q", 'once; hal "none"; select quanta.version;', "--hal", "native").stdout
assert run("get", "hal.name", "--hal", "none", "--format", "value").stdout.strip() == "none"
assert run("get", "hal.capabilities", "--hal", "none", "--format", "value").stdout.strip() == "0"
records = list(csv.DictReader(io.StringIO(run("get", "hal.logical_cpus", "--hal", "none", "--format", "csv").stdout)))
assert records[0]["status"] == "UNSUPPORTED"
run("get", "quanta.version", "--hal", "nonexistent-hal", status=1)
run("hal-info", "--hal", "nonexistent-hal", status=2)
run("get", "quanta.version", "--hal", "", status=2)
run("get", "quanta.version", "--hal", "none", "--hal", "native", status=2)
run("get", "quanta.version", "--allow-hardware-write", status=2)
run("run", "-q", 'hal "none"; hal "native"; select quanta.version;', status=2)
run("run", "-q", 'select provider("os", quanta.version);', status=2)

with tempfile.TemporaryDirectory(prefix="quanta cli ") as directory:
    query = Path(directory) / "\u0437\u0430\u043f\u0440\u043e\u0441 \u0441 \u043f\u0440\u043e\u0431\u0435\u043b\u0430\u043c\u0438.q"
    output = Path(directory) / "\u0440\u0435\u0437\u0443\u043b\u044c\u0442\u0430\u0442.csv"
    query.write_text('version 1; mode cli; once; hal "none"; select quanta.version;', encoding="utf-8")
    assert run(str(query), "--format", "value").stdout == value
    # A CLI mode overrides the query before validation; no GUI dependency is used.
    query.write_text('mode gui; select quanta.version;', encoding="utf-8")
    assert run(str(query), "--cli", "--once", "--format", "value").stdout == value
    run(str(query), "--input", "capture.qblob", status=2)
    run(str(query), "--record", "capture.qblob", status=2)
    run("get", "quanta.version", "--format", "csv", "--output", str(output))
    first = output.read_bytes()
    run("get", "quanta.version", "--output", str(output), status=1)
    assert output.read_bytes() == first, "an existing output file was overwritten"
    assert run("run", "--query-file", "-", "--format", "value",
               input="once; select quanta.version;").stdout == value
    query.write_bytes(b"once; select quanta.version;\0")
    run(str(query), status=2)
    query.write_bytes(b"x" * 65537)
    run(str(query), status=2)

# Every compiled embedded document must preserve its Russian Markdown bytes.
# Minimal builds may intentionally omit documentation, but never silently truncate it.
for line in run("list", "--compiled").stdout.splitlines():
    fields = line.split()
    if not fields:
        continue
    doc_path = ROOT / "docs/metrics" / (fields[0] + ".md")
    if not doc_path.is_file():
        continue
    description = run("describe", fields[0]).stdout
    if "\n---\n" in description:
        embedded = description.split("\n---\n", 1)[1]
        assert embedded == doc_path.read_text(encoding="utf-8") + "\n", fields[0]

run(str(ROOT / "examples/queries/00-portable.q"))
# Product copies are not an implementation of runtime modes.
for name in ("quanta_cli", "quanta_csv", "quanta_txt", "quanta_gui", "quanta_tui"):
    assert not Path(EXE).with_name(name + Path(EXE).suffix).exists(), name
print(f"Single-tool CLI: {checks} command cases passed")
