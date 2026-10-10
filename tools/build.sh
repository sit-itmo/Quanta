#!/bin/sh
# No sudo, installation, or dependency fetch. Run from any directory.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
preset=${1:-linux-default}
case "$preset" in linux-default|linux-minimal|linux-hwmon|linux-asan|portable-posix) ;; *) echo "Unknown Linux preset: $preset" >&2; exit 2;; esac
cd "$root"
cmake --preset "$preset"
cmake --build --preset "$preset" --parallel
printf 'Executable: %s/build/%s/bin/quanta\n' "$root" "$preset"
