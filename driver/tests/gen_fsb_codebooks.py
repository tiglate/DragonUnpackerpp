#!/usr/bin/env python3
"""Generates ../cpp/src/codecs/FsbCodebooks.data.inc (the zlib packed table; the size is PackedSize in FsbCodebooks.cpp): the Vorbis setup packets that FMOD refers to by a CRC32 (extracted from
FMOD by python-fsb5, ISC license; this list is the one of vgmstream's src/coding/libs/vorbis_codebooks_fsb.h).
Usage: gen_fsb_codebooks.py vorbis_codebooks_fsb.h"""
import os, re, struct, sys, zlib
src = open(sys.argv[1]).read()
books = {}
for m in re.finditer(r"static const uint8_t vcb_([0-9a-f]{8})\[\] = \{(.*?)\};", src, re.S):
    books[int(m.group(1), 16)] = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", m.group(2)))
# the list gives the CRC and the size of each
lst = {int(a, 16): int(b, 16) for a, b in re.findall(r"\{0x([0-9a-f]{8}),0x([0-9a-f]{4}),vcb_[0-9a-f]{8}\}", src)}
assert set(lst) == set(books), (len(lst), len(books))
for k, v in lst.items():
    assert len(books[k]) == v, hex(k)
blob = struct.pack("<I", len(books))
data = b""
for crc in sorted(books):
    blob += struct.pack("<III", crc, len(data), len(books[crc]))
    data += books[crc]
pk = zlib.compress(blob + data, 9)
here = os.path.dirname(os.path.abspath(__file__))
rows = []
for i in range(0, len(pk), 24):
    rows.append("    " + ",".join("0x%02X" % b for b in pk[i:i + 24]) + ",")
open(os.path.join(here, "../cpp/src/codecs/FsbCodebooks.data.inc"), "w").write("\n".join(rows) + "\n")
print("%d books, %d bytes unpacked, %d packed: update PackedSize in FsbCodebooks.cpp if it changed" % (len(books), len(blob) + len(data), len(pk)))
