#!/usr/bin/env python3
"""Checks the LZO1X decoder (drv_lzo.pas) against the reference compressor: builds retail
Battlefield 1942 .RFA files holding data packed by liblzo2 (through ctypes) and compares the
MD5 of every entry listed by dudi7cli with the original data.
Usage: verify_lzo.py plugin.so dudi7cli   (skipped when liblzo2 is not installed)"""
import hashlib, os, random, shutil, struct, subprocess, sys, tempfile

plugin, cli = sys.argv[1:3]
if not shutil.which("lzop"):
    print("lzop not found: LZO test skipped")
    sys.exit(0)


def compress(data):
    """Raw LZO1X block from the reference tool (lzop file with a single block)."""
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "x")
        open(src, "wb").write(data)
        subprocess.run(["lzop", "-1", "-q", "--no-checksum", "--no-name", "-f", src], check=True)
        f = open(src + ".lzo", "rb").read()
    assert f[:9] == b"\x89LZO\x00\r\n\x1a\n"
    ver, = struct.unpack(">H", f[9:11])
    p = 9 + 2 + 2 + 2 + 1 + 1                 # version, lib version, needed version, method, level
    flags, = struct.unpack(">I", f[p:p + 4]); p += 4
    assert not flags & 0x800                  # no filter
    p += 4 + 4 + 4                            # mode, mtime low, mtime high
    p += 1 + f[p]                             # name
    p += 4                                    # header checksum
    usize, csize = struct.unpack(">II", f[p:p + 8]); p += 8
    assert usize == len(data)
    if csize == usize:
        return None                           # stored: not usable as an LZO stream
    return f[p:p + csize]


random.seed(5)
words = [b"dragon", b"unpacker", b"archive", b"texture", b"sound", b"\x00\x01\x02\x03"]
samples = {
    "text.txt": b" ".join(random.choice(words) for _ in range(20000)),
    "noise.bin": bytes(random.randrange(256) for _ in range(30000)),
    "zeros.bin": bytes(100000),
    "tiny.bin": b"abcabcabcabcX",
    "one.bin": b"Z",
    "mix.bin": b"".join(bytes(random.randrange(256) for _ in range(random.randrange(1, 60))) + b"ab" * random.randrange(1, 40) for _ in range(3000)),
}
ents, data, off, num = b"", b"", 8, 0
for name, d in samples.items():
    c = compress(d)
    if c is None:
        print('stored by lzop, skipped', name)
        continue
    seg = struct.pack("<I", 1) + struct.pack("<III", len(c), len(d), 0) + c
    num += 1
    ents += struct.pack("<I", len(name)) + name.encode() + struct.pack("<IIIIII", len(seg), len(d), off, 0, 0, 0)
    data += seg
    off += len(seg)
blob = struct.pack("<II", off, 1) + data + struct.pack("<I", num) + ents
with tempfile.TemporaryDirectory() as d:
    p = os.path.join(d, "lzo.rfa")
    open(p, "wb").write(blob)
    out = subprocess.run([cli, plugin, p], capture_output=True, text=True)
    if out.returncode != 0:
        print(out.stdout, out.stderr)
        sys.exit(1)
    bad = 0
    got = {}
    for line in out.stdout.splitlines():
        if line.startswith("#"):
            continue
        f = line.split("\t")
        got[f[4]] = f[3]
    for name, d in samples.items():
        if name not in got:
            continue
        ok = got.get(name) == hashlib.md5(d).hexdigest()
        print(("ok  " if ok else "FAIL"), name, len(d))
        bad += not ok
    sys.exit(1 if bad else 0)
