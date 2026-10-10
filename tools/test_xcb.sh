#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(mktemp -d /tmp/inv-xcb-test.XXXXXX)
trap 'rm -rf "$build"' EXIT HUP INT TERM
cc=${CC:-cc}
archive=$($cc -print-file-name=libxcb.a)
[ "$archive" != libxcb.a ] || { echo 'Install libxcb1-dev (static libxcb.a)' >&2; exit 1; }
"$cc" -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -O2 -Wall -Wextra -Wpedantic -Werror -static \
  -I"$root/src/frontends/imgui" "$root/tests/xcb_smoke.c" \
  "$root/src/frontends/imgui/xcb_platform.c" "$archive" -pthread -o "$build/xcb-smoke"
"$root/tools/verify_static.sh" "$build/xcb-smoke"
python3 "$root/tests/x11_smoke.py" "$build/xcb-smoke"
