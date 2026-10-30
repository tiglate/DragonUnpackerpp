#!/usr/bin/env python3
"""NES / SNES / Mega Drive ROMs: the listing, the raw parts, the deinterleaving of .smd dumps and
every pixel of the tile sheets, against an independent decoder written here.
Usage: verify_roms.py plugin.so dudi7cli"""
import os, subprocess, sys, tempfile
import rom_fixtures as rf

plugin, cli = sys.argv[1:3]


def listing(path):
    out = subprocess.run([cli, plugin, path], capture_output=True, check=True).stdout.decode()
    lines = out.splitlines()
    return lines[0], [l.split("\t") for l in lines[1:]]


def extract(path, index, outdir):
    for f in os.listdir(outdir):
        os.remove(os.path.join(outdir, f))
    subprocess.run([cli, plugin, path, "%d-%d" % (index, index), outdir], capture_output=True, check=True)
    return open(os.path.join(outdir, os.listdir(outdir)[0]), "rb").read()


def tga_pixels(tga):
    w, h = tga[12] | tga[13] << 8, tga[14] | tga[15] << 8
    assert tga[2] == 2 and tga[16] == 32 and tga[17] == 0x28 and len(tga) == 18 + w * h * 4
    return w, h, [tga[18 + i * 4] for i in range(w * h)]


def decode(data, kind):
    bpp = 2 if kind in ("nes", "snes2") else 4
    tb = bpp * 8
    tiles = (len(data) + tb - 1) // tb
    h = (tiles + 15) // 16 * 8
    px = [0] * (128 * h)
    for t in range(tiles):
        tile = data[t * tb:(t + 1) * tb].ljust(tb, b"\0")
        for y in range(8):
            for x in range(8):
                if kind == "nes":
                    v = (tile[y] >> (7 - x) & 1) | (tile[8 + y] >> (7 - x) & 1) << 1
                elif kind == "snes4":
                    v = sum((tile[(0 if p < 2 else 16) + 2 * y + (p & 1)] >> (7 - x) & 1) << p for p in range(4))
                else:
                    b = tile[4 * y + x // 2]
                    v = b & 15 if x & 1 else b >> 4
                px[(t // 16 * 8 + y) * 128 + t % 16 * 8 + x] = v * 255 // ((1 << bpp) - 1)
    return h, px


fails = 0


def check(cond, msg):
    global fails
    print(("ok   " if cond else "FAIL ") + msg)
    fails += not cond


with tempfile.TemporaryDirectory() as td:
    out = os.path.join(td, "o")
    os.mkdir(out)
    cases = [("test.nes", rf.nes(), "NES", "nes"), ("test.sfc", rf.snes(), "SNES", "snes4"),
             ("copier.smc", rf.snes(True), "SNES", "snes4"), ("test.md", rf.megadrive(), "MD", "md"),
             ("test.smd", rf.smd(), "MDSMD", "md")]
    for name, (blob, rom), fid, kind in cases:
        path = os.path.join(td, name)
        open(path, "wb").write(blob)
        head, rows = listing(path)
        check(head.startswith("#" + fid), "%s: format %s" % (name, head))
        names = [r[4] for r in rows]
        idx = {n: i for i, n in enumerate(names)}
        info = extract(path, idx["info.txt"], out).decode()
        check(info.count("\n") >= 5, "%s: info.txt (%d lines)" % (name, info.count("\n")))
        rawname = "ROM.sfc" if kind == "snes4" else "ROM.bin"
        if kind == "nes":
            check(extract(path, idx["PRG-ROM.bin"], out) == blob[16:16 + 16384], "%s: PRG" % name)
            chr_ = extract(path, idx["CHR-ROM.bin"], out)
            check(chr_ == rom, "%s: CHR" % name)
            sheet = extract(path, idx["CHR/chr_004000.tga"], out)
            data = chr_
        else:
            check(extract(path, idx[rawname], out) == rom, "%s: ROM image identical to the plain ROM" % name)
            sheet = extract(path, idx["graphics/%s_000000.tga" % ("tiles4bpp" if kind == "snes4" else "tiles")], out)
            data = rom[:32768]
        w, h, px = tga_pixels(sheet)
        eh, epx = decode(data, kind)
        check(h == eh and px == epx, "%s: every pixel of the first sheet (%dx%d)" % (name, w, h))
        entry_size = int(rows[idx["CHR/chr_004000.tga" if kind == "nes" else
                                    "graphics/%s_000000.tga" % ("tiles4bpp" if kind == "snes4" else "tiles")]][2])
        check(entry_size == len(sheet), "%s: listed size = extracted size" % name)
    # a plain file that is not a ROM must not be claimed
    junk = os.path.join(td, "junk.sfc")
    open(junk, "wb").write(rf.pattern(40000))
    r = subprocess.run([cli, plugin, junk], capture_output=True)
    check(r.returncode != 0 or not r.stdout.startswith((b"#SNES", b"#NES", b"#MD")), "random data is not a ROM")
    # compressed streams (Nemesis, Kosinski) of a Mega Drive ROM: found, unpacked, drawn
    blob, streams = rf.md_compressed()
    path = os.path.join(td, "packed.md")
    open(path, "wb").write(blob)
    head, rows = listing(path)
    idx = {r[4]: i for i, r in enumerate(rows)}
    for codec, at, data in streams:
        base = "%s/%s_%06X" % (codec, "art" if codec == "nemesis" else "data", at)
        check(base + ".bin" in idx, "%s at %X found" % (codec, at))
        if base + ".bin" not in idx:
            continue
        check(extract(path, idx[base + ".bin"], out) == data, "%s at %X: data identical (%d bytes)" % (codec, at, len(data)))
        w, h, px = tga_pixels(extract(path, idx[base + ".tga"], out))
        eh, epx = decode(data, "md")
        check(h == eh and px == epx, "%s at %X: sheet pixels" % (codec, at))
    check(sum(1 for r in rows if r[4].endswith(".bin") and "/" in r[4]) == len(streams), "no stream found besides the 4 real ones")
print("ALL OK" if not fails else "%d FAILURE(S)" % fails)
sys.exit(1 if fails else 0)
