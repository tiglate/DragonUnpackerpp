#!/usr/bin/env python3
"""Writes a Quake PAK with many entries to stress the browser: make_big_pak.py out.pak [entries] [folders]"""
import struct, sys

out = sys.argv[1]
n = int(sys.argv[2]) if len(sys.argv) > 2 else 300000
folders = int(sys.argv[3]) if len(sys.argv) > 3 else 1600
payload = b"x" * 16
ents = []
data = b""
for i in range(n):
    name = ("dir%04d/sub%02d/file%06d.dat" % (i % folders, (i // folders) % 5, i)).encode()[:55]
    ents.append((name, 12 + len(data), len(payload)))
    data += payload
d = b"".join(struct.pack("<56sII", nm.ljust(56, b"\0"), o, s) for nm, o, s in ents)
with open(out, "wb") as f:
    f.write(b"PACK" + struct.pack("<II", 12 + len(data), len(d)) + data + d)
