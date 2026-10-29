#!/bin/sh
# Builds the two test tools (texdec_dump, crn_dump) and compares the texture decoders with texture2ddecoder
# (pip install texture2ddecoder; or TEXTURE2DDECODER_PATH=folder). Skipped when the package is missing.
set -e
here=$(cd "$(dirname "$0")" && pwd)
cpp=$here/../cpp/src
out=${TMPDIR:-/tmp}/dup-texdec.$$
mkdir -p "$out"
trap 'rm -rf "$out"' EXIT
g++ -std=c++20 -O1 -o "$out/texdec_dump" "$here/texdec_dump.cpp" "$cpp/codecs/TexDecode.cpp" "$cpp/codecs/Astc.cpp" || exit 1
g++ -std=c++20 -O1 -o "$out/crn_dump" "$here/crn_dump.cpp" "$cpp/codecs/Crunch.cpp" || exit 1
python3 -I "$here/verify_texdecode.py" "$out/texdec_dump" "$out/crn_dump"
