#!/bin/sh
# Runs the automated tests that need no game files:
#   run.sh      the driver contract (C harness + Pascal host) on synthetic fixtures
#   fuzz.py     mutated fixtures must never crash or hang the driver
#   verify_lzo.py  LZO1X decoder vs the reference compressor (needs lzop)
#   verify_lzma.py LZMA decoder vs liblzma (Python lzma module)
#   zero_entries.py archives with 0 entries must open (unsigned loop bug)
#   verify_real.py + unity_ref.py on the synthetic Unity bundles with serialized files
# Optional (need Xvfb and real archives): run_real.sh, sweep.py
set -e
here=$(cd "$(dirname "$0")" && pwd)
"$here/run.sh" | tail -2
out=${TMPDIR:-/tmp}/dup-runall.$$
mkdir -p "$out/o"
trap 'rm -rf "$out"' EXIT
root=$here/../..
# The plugin under test (./make.sh build driver), DUDI7_PLUGIN=file to try another one
plugin=${DUDI7_PLUGIN:-$root/build/plugins/libdrv_default.d6d.so}
cli=$root/build/tools/dudi7cli
python3 "$here/make_fixtures.py" "$out/fx"
python3 "$here/fuzz.py" "$plugin" "$cli" "$out/fx" 40
python3 "$here/verify_roms.py" "$plugin" "$cli" | tail -1
python3 "$here/verify_lzo.py" "$plugin" "$cli" | tail -3
python3 "$here/verify_lzma.py" "$plugin" "$cli" | tail -3
# objects of the serialized files of the bundles: the listing and the MD5 of every entry vs unity_ref.py
for f in obj17 obj19 obj21 obj22 obj22c ufs22 notree0 notree1 notree2 notree3 texfmts objs3 objs3_notree; do
  "$cli" "$plugin" "$out/fx/$f.bundle" >"$out/$f.lst"
  python3 "$here/verify_real.py" "$out/fx/$f.bundle" "$out/$f.lst" >"$out/$f.ver" || { cat "$out/$f.ver"; exit 1; }
  sed 's/^/unity objects: /' "$out/$f.ver"
done
# FMOD banks: listing and MD5 of every entry vs the python reader of verify_real.py
for f in test.fsb mpeg.fsb test.bank vorbis.fsb vorbis.bank empty.bank; do
  "$cli" "$plugin" "$out/fx/$f" >"$out/$f.lst"
  python3 "$here/verify_real.py" "$out/fx/$f" "$out/$f.lst" >"$out/$f.ver" || { cat "$out/$f.ver"; exit 1; }
  sed 's/^/fmod: /' "$out/$f.ver"
done
# XNA content files: converted entries vs verify_real.py; the LZX ones (made of stored blocks) must give the same data as their uncompressed twin
for f in tex dxt rgb565 snd custom lz4 font; do
  "$cli" "$plugin" "$out/fx/$f.xnb" >"$out/$f.lst"
  python3 "$here/verify_real.py" "$out/fx/$f.xnb" "$out/$f.lst" >"$out/$f.ver" || { cat "$out/$f.ver"; exit 1; }
  sed 's/^/xnb: /' "$out/$f.ver"
done
for f in lzx lzx_small font_lzx; do
  ref=${f}_ref
  "$cli" "$plugin" "$out/fx/$f.xnb" | cut -f3,4 | head -1 >"$out/$f.lst"
  "$cli" "$plugin" "$out/fx/$ref.xnb" | cut -f3,4 | head -1 >"$out/$ref.lst"
  cmp -s "$out/$f.lst" "$out/$ref.lst" && echo "xnb: $f.xnb (LZX): same data as the uncompressed file" || { echo "xnb: $f.xnb: LZX data differs"; exit 1; }
done
# the converted textures (crunch, ETC/EAC, ASTC, 16 bits) must keep the MD5s that texture2ddecoder agreed with
"$cli" "$plugin" "$out/fx/texfmts.bundle" | grep -v '^#' | cut -f4,5 | sed '/^$/d' >"$out/texfmts.got"
cmp -s "$out/texfmts.got" "$here/texfmts.expected" && echo "textures: converted data unchanged" || { echo "textures: converted data differs from driver/tests/texfmts.expected"; diff "$out/texfmts.got" "$here/texfmts.expected" | head; exit 1; }
# decoders against texture2ddecoder (skipped when it is not installed)
"$here/verify_texdecode.sh" | tail -2
# bundles with no file and no (or one empty) block must open with 0 entries (unsigned loop counters)
for f in empty8.bundle empty6.unity3d; do
  timeout 20 "$cli" "$plugin" "$out/fx/$f" list >"$out/$f.lst" 2>&1 || { echo "empty bundle $f: failed"; cat "$out/$f.lst"; exit 1; }
  grep -q "^#UnityFS.0$" "$out/$f.lst" || { echo "empty bundle $f: not opened with 0 entries"; exit 1; }
  echo "empty bundle $f: OK"
done
python3 "$here/zero_entries.py" "$plugin" "$cli" "$out/fx" | tail -3
