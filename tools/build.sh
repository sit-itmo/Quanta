#!/bin/sh
# Explicit --gui enables the pinned dependency fetch. No sudo or system changes.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
gui=OFF
case ${1:-} in
  '') build="$root/build" ;;
  --gui) gui=ON; build="$root/build-gui" ;;
  *) echo "usage: $0 [--gui]" >&2; exit 2 ;;
esac
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DINV_WITH_GUI="$gui" -DINV_STRICT_STATIC=ON
cmake --build "$build" --parallel
ctest --test-dir "$build" --output-on-failure
"$root/tools/verify_static.sh" "$build/inv"
printf 'Ready: %s/inv\n' "$build"
