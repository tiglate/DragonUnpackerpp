#!/usr/bin/env python3
"""NES / SNES / Mega Drive ROMs: the listing, the raw parts, the deinterleaving of .smd dumps and
every pixel of the tile sheets, against an independent decoder written here.
Usage: verify_roms.py plugin.so dudi7cli"""
import os, struct, subprocess, sys, tempfile
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
    """(width, height, gray level of each pixel) of a tile sheet: a colour mapped TGA of 8 bits, the map of
    16 grays; the 32 bits swatches are read by tga_colors"""
    w, h = tga[12] | tga[13] << 8, tga[14] | tga[15] << 8
    assert tga[1] == 1 and tga[2] == 1 and tga[7] == 24 and tga[16] == 8 and tga[17] == 0x20
    assert (tga[5] | tga[6] << 8) == 16 and len(tga) == 18 + 48 + w * h
    gray = [tga[18 + 3 * i] for i in range(16)]
    assert all(tga[18 + 3 * i] == tga[19 + 3 * i] == tga[20 + 3 * i] for i in range(16))
    return w, h, [gray[tga[66 + i]] for i in range(w * h)]


def tga_colors(tga):
    """The (r, g, b) of each 16 pixel square of a swatch (32 bits TGA, 16 squares per row)"""
    w, h = tga[12] | tga[13] << 8, tga[14] | tga[15] << 8
    assert tga[2] == 2 and tga[16] == 32 and len(tga) == 18 + w * h * 4 and w == 256
    colors = []
    for row in range(h // 16):
        for col in range(16):
            at = 18 + ((row * 16 + 8) * w + col * 16 + 8) * 4
            colors.append((tga[at + 2], tga[at + 1], tga[at]))
    return colors


def decode(data, kind):
    bpp = 2 if kind in ("nes", "snes2") else 3 if kind == "snes3" else 4
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
                elif kind == "snes3":
                    v = sum(((tile[2 * y + p] if p < 2 else tile[16 + y]) >> (7 - x) & 1) << p for p in range(3))
                elif kind == "snes4":
                    v = sum((tile[(0 if p < 2 else 16) + 2 * y + (p & 1)] >> (7 - x) & 1) << p for p in range(4))
                else:
                    b = tile[4 * y + x // 2]
                    v = b & 15 if x & 1 else b >> 4
                px[(t // 16 * 8 + y) * 128 + t % 16 * 8 + x] = v * 255 // ((1 << bpp) - 1)
    return h, px


MDL = [0, 52, 87, 116, 144, 172, 206, 255]
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
            w, h, px = tga_pixels(sheet)
            eh, epx = decode(data, kind)
            check(h == eh and px == epx, "%s: every pixel of the first sheet (%dx%d)" % (name, w, h))
            check(int(rows[idx["CHR/chr_004000.tga"]][2]) == len(sheet), "%s: listed size = extracted size" % name)
        else:
            check(extract(path, idx[rawname], out) == rom, "%s: ROM image identical to the plain ROM" % name)
            check(not any(n.startswith("graphics/") for n in names), "%s: no raw tile sheets (noise for compressed games)" % name)
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
        if codec == "nemesis" or (len(data) >= 2048 and len(data) % 32 == 0):
            w, h, px = tga_pixels(extract(path, idx[base + ".tga"], out))
            eh, epx = decode(data, "md")
            check(h == eh and px == epx, "%s at %X: sheet pixels" % (codec, at))
        else:
            check(base + ".tga" not in idx, "%s at %X: no sheet for %d bytes (a table, not tiles)" % (codec, at, len(data)))
    check(sum(1 for r in rows if r[4].endswith(".bin") and "/" in r[4]) == len(streams), "no stream found besides the 4 real ones")
    # Nintendo LZ streams of a Super Nintendo ROM
    for variant in ("little", "big", "metroid"):
      blob, streams = rf.snes_compressed(variant)
      path = os.path.join(td, "packed.sfc")
      open(path, "wb").write(blob)
      head, rows = listing(path)
      idx = {r[4]: i for i, r in enumerate(rows)}
      for at, data in streams:
          base = "compressed/lz_%06X" % at
          check(base + ".bin" in idx, "lz-%s at %X found" % (variant, at))
          if base + ".bin" not in idx:
              continue
          check(extract(path, idx[base + ".bin"], out) == data, "lz at %X: data identical (%d bytes)" % (at, len(data)))
          for suffix, kind in (("_4bpp", "snes4"), ("_3bpp", "snes3")):
              if base + suffix + ".tga" in idx:
                  w, h, px = tga_pixels(extract(path, idx[base + suffix + ".tga"], out))
                  eh, epx = decode(data, kind)
                  check(h == eh and px == epx, "lz at %X: %s sheet pixels" % (at, suffix[1:]))
      check(sum(1 for r in rows if r[4].endswith(".bin") and r[4].startswith("compressed/")) == len(streams), "no stream besides the real ones")
    # the LZSS of Final Fantasy VI: blocks with their size, found through a table of addresses
    for counts in (True, False):
        blob, streams = rf.snes_lzss(counts)
        path = os.path.join(td, "sized.sfc")
        open(path, "wb").write(blob)
        head, rows = listing(path)
        idx = {r[4]: i for i, r in enumerate(rows)}
        for at, data in streams:
            base = "compressed/lzss_%06X" % at
            check(base + ".bin" in idx, "lzss (header %s) at %X found" % ("counts itself" if counts else "does not", at))
            if base + ".bin" in idx:
                check(extract(path, idx[base + ".bin"], out) == data, "lzss at %X: data identical (%d bytes)" % (at, len(data)))
        check(sum(1 for r in rows if r[4].startswith("compressed/lzss_") and r[4].endswith(".bin")) == len(streams), "lzss: no block besides the real ones")
        w, h, px = tga_pixels(extract(path, idx["compressed/lzss_%06X_4bpp.tga" % streams[0][0]], out))
        eh, epx = decode(streams[0][1], "snes4")
        check(h == eh and px == epx, "lzss: 4bpp sheet pixels")
    # BRR samples of a Super Nintendo ROM: WAV files identical to the reference decoder
    blob, samples = rf.snes_brr()
    path = os.path.join(td, "brr.sfc")
    open(path, "wb").write(blob)
    head, rows = listing(path)
    wavs = [r for r in rows if r[4].startswith("audio/")]
    check(len(wavs) == len(samples), "BRR: %d samples found, %d expected" % (len(wavs), len(samples)))
    for at, loop, nblocks, pcm in samples:
        name = "audio/brr_%06X%s.wav" % (at, "_loop" if loop else "")
        idx = {r[4]: i for i, r in enumerate(rows)}
        check(name in idx, "BRR %s listed" % name)
        if name not in idx:
            continue
        wav = extract(path, idx[name], out)
        check(int(rows[idx[name]][2]) == len(wav), "BRR %06X: listed size = file size" % at)
        check(wav[:4] == b"RIFF" and wav[8:12] == b"WAVE" and struct.unpack("<HHI", wav[20:28]) == (1, 1, 32000) and
              struct.unpack("<H", wav[34:36])[0] == 16, "BRR %06X: WAV header (PCM, mono, 32 kHz, 16 bits)" % at)
        check(list(struct.unpack("<%dh" % len(pcm), wav[44:])) == pcm, "BRR %06X: %d samples identical to the reference decoder" % (at, len(pcm)))
    # palettes: found where they are, as .pal files and swatches, with the right colors
    expect_md = {0x3000 + 32 * k: [(MDL[(w >> 1) & 7], MDL[(w >> 5) & 7], MDL[(w >> 9) & 7]) for w in words]
                 for k, words in enumerate(rf.md_palettes()[1])}
    expect_snes = {0x3000 + 32 * k: [(((w & 31) << 3) | ((w & 31) >> 2), (((w >> 5) & 31) << 3) | (((w >> 5) & 31) >> 2),
                                      (((w >> 10) & 31) << 3) | (((w >> 10) & 31) >> 2)) for w in words]
                   for k, words in enumerate(rf.snes_palettes()[1])}
    for name, blob, expect in (("pal.md", rf.md_palettes()[0], expect_md), ("pal.sfc", rf.snes_palettes()[0], expect_snes)):
        path = os.path.join(td, name)
        open(path, "wb").write(blob)
        head, rows = listing(path)
        idx = {r[4]: i for i, r in enumerate(rows)}
        found = sorted(int(n[len("palettes/pal_"):-4], 16) for n in idx if n.startswith("palettes/") and n.endswith(".pal"))
        check(all(o in found for o in expect), "%s: the %d palette lines are found (%d palettes listed)" % (name, len(expect), len(found)))
        for o, colors in expect.items():
            base = "palettes/pal_%06X" % o
            if base + ".pal" not in idx:
                continue
            text = extract(path, idx[base + ".pal"], out).decode().split("\r\n")
            got = [tuple(int(v) for v in l.split()) for l in text[3:3 + 16]]
            check(text[0] == "JASC-PAL" and text[2] == "16" and got == colors, "%s: palette at %X, JASC file with the right colors" % (name, o))
            check(tga_colors(extract(path, idx[base + ".tga"], out)) == colors, "%s: palette at %X, swatch with the right colors" % (name, o))
            check(int(rows[idx[base + ".tga"]][2]) == 18 + 256 * 16 * 4, "%s: swatch listed size" % name)
    path = os.path.join(td, "pal.nes")
    open(path, "wb").write(rf.nes_palettes())
    head, rows = listing(path)
    idx = {r[4]: i for i, r in enumerate(rows)}
    names = ["palettes/pal_%06X.pal" % (0x1000 + 4 * g) for g in range(4)]
    check(all(n in idx for n in names), "pal.nes: the 4 palettes of the tiles are found at 0x1000, 0x1004...")
    if all(n in idx for n in names):
        pals = [[tuple(int(v) for v in l.split()) for l in extract(path, idx[n], out).decode().split("\r\n")[3:7]] for n in names]
        check(pals[0][0] == (0, 0, 0) and pals[0][1] == (252, 252, 252) and pals[0][2] == (60, 188, 252), "pal.nes: colors of the first palette (0F 30 21 11)")
    # sampled sound of a Mega Drive ROM: found, unsigned 8 bits WAV with the samples as they are
    blob, samples = rf.md_pcm()
    path = os.path.join(td, "pcm.md")
    open(path, "wb").write(blob)
    head, rows = listing(path)
    idx = {r[4]: i for i, r in enumerate(rows)}
    check("audio/pcm_003000.wav" in idx, "PCM at 0x3000 found (%d samples listed)" % sum(1 for r in rows if r[4].startswith("audio/")))
    if "audio/pcm_003000.wav" in idx:
        wav = extract(path, idx["audio/pcm_003000.wav"], out)
        check(int(rows[idx["audio/pcm_003000.wav"]][2]) == len(wav), "PCM: listed size = file size")
        check(wav[:4] == b"RIFF" and struct.unpack("<HHI", wav[20:28]) == (1, 1, 10000) and struct.unpack("<H", wav[34:36])[0] == 8, "PCM: WAV header (PCM, mono, 10 kHz, 8 bits)")
        check(wav[44:] == samples, "PCM: the %d samples are identical to the ROM" % len(samples))
print("ALL OK" if not fails else "%d FAILURE(S)" % fails)
sys.exit(1 if fails else 0)
