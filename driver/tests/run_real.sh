#!/bin/sh
# Optional: runs the default driver on real game files and compares with an
# independent python reader. Usage: run_real.sh file...   (files are copied to
# a temporary directory first, the originals are never opened by the tests)
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$here/../..
out=${TMPDIR:-/tmp}/dudi7-real.$$
mkdir -p "$out"
trap 'rm -rf "$out"' EXIT
plugin=$root/build/plugins/libdrv_default.d6d.so
rc=0
for f in "$@"; do
  base=$(basename "$f")
  case "$base" in *.*) ext=${base##*.} ;; *) ext=dat ;; esac      # many Unity bundles have no extension
  cp "$f" "$out/archive.$ext"
  "$root/build/tools/dudi7cli" "$plugin" "$out/archive.$ext" > "$out/listing"
  python3 "$here/verify_real.py" "$out/archive.$ext" "$out/listing" | sed "s|^archive.$ext|$(basename "$f")|" || rc=1
  rm "$out/archive.$ext"
done
exit $rc
