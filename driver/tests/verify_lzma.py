#!/usr/bin/env python3
"""Checks the LZMA decoder (drv_lzma.pas) against liblzma (Python's lzma module): builds
Ghostbusters .PAK files holding data of several kinds packed with several lc/lp/pb settings and
compares the MD5 of every entry listed by dudi7cli with the original data.
Usage: verify_lzma.py plugin.so dudi7cli"""
import hashlib, lzma, os, random, struct, subprocess, sys, tempfile

plugin, cli = sys.argv[1:3]
random.seed(9)
words = [b"dragon", b"unpacker", b"archive", b"texture", b"sound", b"\x00\x01\x02\x03", b"LZMA"]
samples = {
    "text.txt": b" ".join(random.choice(words) for _ in range(30000)),
    "noise.bin": bytes(random.randrange(256) for _ in range(40000)),
    "zeros.bin": bytes(200000),
    "tiny.bin": b"abcabcabcabcX",
    "one.bin": b"Z",
    "mix.bin": b"".join(bytes(random.randrange(256) for _ in range(random.randrange(1, 60))) + b"ab" * random.randrange(1, 40) for _ in range(4000)),
    "long.bin": bytes((i * i) & 255 for i in range(300000)),
}
settings = [(3, 0, 2), (0, 4, 0), (4, 0, 4), (2, 2, 1), (1, 1, 1)]
names, blobs, md5 = [], [], {}
for i, (name, data) in enumerate(samples.items()):
    lc, lp, pb = settings[i % len(settings)]
    f = lzma.compress(data, format=lzma.FORMAT_ALONE,
                      filters=[{"id": lzma.FILTER_LZMA1, "preset": 4 + i % 5, "lc": lc, "lp": lp, "pb": pb}])
    f = f[:5] + struct.pack("<Q", len(data)) + f[13:]
    nm = "%s_%d%d%d" % (name, lc, lp, pb)
    names.append(nm)
    blobs.append(f)
    md5[nm] = hashlib.md5(data).hexdigest()
dirlen = sum(12 + 4 + len(n) for n in names)
off, d, body = 26 + dirlen, b"", b""
for n, b in zip(names, blobs):
    d += struct.pack("<III", off, len(b), 1) + struct.pack("<I", len(n)) + n.encode()
    body += b
    off += len(b)
blob = b"tongas_pack_v30000" + struct.pack("<II", len(names), dirlen) + d + body
with tempfile.TemporaryDirectory() as t:
    p = os.path.join(t, "lzma.pak")
    open(p, "wb").write(blob)
    out = subprocess.run([cli, plugin, p], capture_output=True, text=True)
    if out.returncode != 0:
        print(out.stdout, out.stderr)
        sys.exit(1)
    got = {}
    for line in out.stdout.splitlines():
        if not line.startswith("#"):
            f = line.split("\t")
            got[f[4]] = f[3]
    bad = 0
    for n in names:
        ok = got.get(n) == md5[n]
        print(("ok  " if ok else "FAIL"), n)
        bad += not ok
    sys.exit(1 if bad else 0)
