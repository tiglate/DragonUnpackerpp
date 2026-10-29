#!/bin/sh
# Builds the default driver, then drives it from C (harness.c, no Pascal on the
# host side). The host side in C++ (Driver/Archive of the interface) is covered by the unit tests of app/.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$here/../..
# The plugin under test (./make.sh build driver), DUDI7_PLUGIN=file to try another one
plugin=${DUDI7_PLUGIN:-$root/build/plugins/libdrv_default.d6d.so}
out=${TMPDIR:-/tmp}/dudi7-test.$$
mkdir -p "$out"
trap 'rm -rf "$out"' EXIT
python3 "$here/make_fixtures.py" "$out/fx"
gcc -Wall -Wextra -Wno-missing-field-initializers -I"$root/driver/contract" -o "$out/harness" "$here/harness.c" -ldl

"$out/harness" "$plugin" "$out/fx"
