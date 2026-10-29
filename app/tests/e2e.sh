#!/bin/sh
# End to end: the real Qt application (offscreen platform, no display needed) extracts whole archives and the MD5 of
# every extracted file is compared with what the driver lists .
# Usage: e2e.sh archive...   (the files are copied to a temporary directory first, the originals are never opened)
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$here/../..
app=$here/../build/src/dragonunpacker
cli=$root/build/tools/dudi7cli
plugin=$root/build/plugins/libdrv_default.d6d.so
out=${TMPDIR:-/tmp}/dup-qt-e2e.$$
mkdir -p "$out"
trap 'rm -rf "$out"' EXIT
rc=0
for f in "$@"; do
  name=$(basename "$f")
  mkdir -p "$out/in" "$out/dest"
  cp "$f" "$out/in/$name"
  "$cli" "$plugin" "$out/in/$name" > "$out/listing"
  if ! DUP_PLUGIN="$plugin" timeout 600 "$app" --settings="$out/settings.ini" --open="$out/in/$name" \
       --extract-all="$out/dest" --quit -platform offscreen >"$out/log" 2>&1; then
    echo "$name: the application failed"; tail -5 "$out/log"; rc=1
  fi
  python3 "$root/driver/tests/compare_extract.py" "$out/listing" "$out/dest" "$name" || rc=1
  rm -rf "$out/in" "$out/dest"
done
exit $rc
