#!/bin/sh
# Verify the executable itself, not merely the type of internal libraries.
set -eu
binary=${1:-build/static/bin/quanta}
command -v readelf >/dev/null 2>&1 || { echo 'readelf (binutils) is required' >&2; exit 1; }
[ -f "$binary" ] || { echo "Not found: $binary" >&2; exit 1; }
headers=$(LC_ALL=C readelf -l "$binary")
dynamic=$(LC_ALL=C readelf -d "$binary")
if printf '%s\n' "$headers" | grep -q 'INTERP'; then
  echo "FAIL: $binary contains a program interpreter" >&2; exit 1
fi
if printf '%s\n' "$dynamic" | grep -q '(NEEDED)'; then
  echo "FAIL: $binary contains dynamic library dependencies" >&2; exit 1
fi
printf 'PASS: %s has no PT_INTERP and no DT_NEEDED\n' "$binary"
