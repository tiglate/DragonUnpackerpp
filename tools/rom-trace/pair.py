#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""pair.py ROM TRACE.jsonl OUT.json --title NAME [--build DIR]: from the trace of a game, finds for the tiles that the screen
uses (backgrounds and sprites) the ROM data they come from (a stream that the driver decodes, or uncompressed bytes of the ROM)
and the palette they were drawn with, and writes the metadata table (data/rom-metadata/FORMAT.md). No game bytes are written:
only offsets, sizes, codec names and colours. Needs the built driver (./make.sh build driver)."""
import argparse, binascii, collections, json, os, re, subprocess, sys, tempfile

ap = argparse.ArgumentParser()
ap.add_argument("rom"); ap.add_argument("trace"); ap.add_argument("out")
ap.add_argument("--title", required=True)
ap.add_argument("--build", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "build"))
ap.add_argument("--max-palettes", type=int, default=6)
a = ap.parse_args()

rom_bytes = open(a.rom, "rb").read()
if len(rom_bytes) % 1024 == 512: rom_bytes = rom_bytes[512:]
crc = "%08x" % (binascii.crc32(rom_bytes) & 0xFFFFFFFF)
cli = [os.path.join(a.build, "tools", "dudi7cli"), os.path.join(a.build, "plugins", "libdrv_default.d6d.so"), a.rom]
listing = subprocess.run(cli + ["list"], capture_output=True, text=True).stdout.splitlines()
tmp = tempfile.mkdtemp(prefix="rom-trace-streams-")
ids = [int(l.split("\t")[0]) for l in listing if re.search(r"compressed/\w+\.bin", l)]
for i in ids: subprocess.run(cli + [f"{i}-{i}", tmp], capture_output=True)
blobs = {}   # stream name (codec_OFFSET) -> decoded bytes
for root, _, files in os.walk(tmp):
    for f in files:
        m = re.match(r"\d+_(\w+)\.bin", f)
        if m: blobs[m.group(1)] = open(os.path.join(root, f), "rb").read()

def to4(t3):   # 24 bytes of 3bpp -> 4bpp with an empty fourth plane
    out = bytearray(t3[:16])
    for y in range(8): out += bytes([t3[16 + y], 0])
    return bytes(out)
index4, index2 = {}, {}   # tile -> (stream, tile number, bpp)
for src, b in blobs.items():
    for size, conv, bpp in ((32, bytes, 4), (24, to4, 3)):
        for k in range(len(b) // size):
            t = conv(b[k * size:(k + 1) * size])
            if any(t): index4.setdefault(t, (src, k, bpp))
    for k in range(len(b) // 16):
        t = b[k * 16:(k + 1) * 16]
        if any(t): index2.setdefault(t, (src, k, 2))
rom_cache = {}
def in_rom(t):
    if t not in rom_cache: rom_cache[t] = rom_bytes.find(t)
    return rom_cache[t]

def color(w):
    r, g, b = w & 31, (w >> 5) & 31, (w >> 10) & 31
    return "%02x%02x%02x" % (r * 255 // 31, g * 255 // 31, b * 255 // 31)

recs = [json.loads(l) for l in open(a.trace)]
vrams = {r["frame"]: bytes.fromhex(r["data"]) for r in recs if r["t"] == "vram"}
cgrams = [(r["frame"], bytes.fromhex(r["data"])) for r in recs if r["t"] == "cgram"]
def cgram_at(frame):
    best = None
    for f, d in cgrams:
        if f <= frame: best = d
    return best

class Entry:
    def __init__(self): self.palettes = {}; self.tiles = set(); self.kinds = set(); self.bpp = set(); self.offsets = set()
table = collections.defaultdict(Entry)   # stream name, or "rom"
stats = collections.Counter()
for r in recs:
    if r["t"] not in ("bg", "oam") or r["frame"] not in vrams: continue
    vram, cg = vrams[r["frame"]], cgram_at(r["frame"])
    if cg is None: continue
    used = []   # (VRAM byte address, tile size, palette number, first CGRAM colour, kind)
    if r["t"] == "bg":
        mode, bg = r["mode"], r["bg"]
        bpp = 2 if mode == 0 or (mode == 1 and bg == 3) else 4
        size = 16 if bpp == 2 else 32
        for k in r["tiles"]:
            tile, pal = k >> 3, k & 7
            used.append((((r["chr"] + tile * (size // 2)) * 2) & 0xFFFF, size, pal, pal * 4 if bpp == 2 else pal * 16, f"bg{bg}"))
    else:
        oam = bytes.fromhex(r["data"]); seen = set()
        for i in range(128):
            x, y, tile, attr = oam[i * 4:i * 4 + 4]
            key = (tile, attr & 1, (attr >> 1) & 7)
            if key in seen: continue
            seen.add(key)
            wa = (r["name"] + tile * 16 + (attr & 1) * r["gap"]) & 0x7FFF
            used.append(((wa * 2) & 0xFFFF, 32, (attr >> 1) & 7, 128 + ((attr >> 1) & 7) * 16, "sprite"))
    for addr, size, pal, cgbase, kind in used:
        t = vram[addr:addr + size]
        if len(t) < size or not any(t): continue
        stats["tiles"] += 1
        hit = (index4 if size == 32 else index2).get(t)
        if hit: src, tileno, bpp = hit; off = None
        else:
            off = in_rom(t)
            if off < 0: continue
            src, bpp = "rom", (4 if size == 32 else 2)
        stats["explained"] += 1
        e = table[src]
        n = 16 if size == 32 else 4
        colors = [int.from_bytes(cg[(cgbase + c) * 2:(cgbase + c) * 2 + 2], "little") for c in range(n)]
        key = " ".join(color(c) for c in colors)
        where = in_rom(cg[cgbase * 2:(cgbase + n) * 2])
        p = e.palettes.setdefault(key, {"count": 0, "romOffset": where if where >= 0 else None})
        p["count"] += 1
        e.kinds.add(kind); e.bpp.add(bpp)
        if off is not None: e.offsets.add((off, size))
        else: e.tiles.add(tileno)

streams, raw = [], []
for src, e in sorted(table.items()):
    pals = [{"colors": k.split(), "romOffset": v["romOffset"], "count": v["count"]}
            for k, v in sorted(e.palettes.items(), key=lambda kv: -kv[1]["count"])[:a.max_palettes]]
    if src == "rom":
        runs = []   # tiles found at ROM offsets, merged into runs of consecutive tiles
        for off, size in sorted(e.offsets):
            if runs and runs[-1][2] == size and off == runs[-1][0] + runs[-1][1]: runs[-1][1] += size
            else: runs.append([off, size, size])
        raw = [{"offset": o, "length": n, "bpp": 4 if s == 32 else 2} for o, n, s in runs]
        raw_pals = pals
        continue
    m = re.match(r"(\w+?)_([0-9A-Fa-f]+)$", src)
    streams.append({"codec": m.group(1), "offset": int(m.group(2), 16), "bpp": sorted(e.bpp),
                    "usedBy": sorted(e.kinds), "tilesSeen": len(e.tiles), "palettes": pals})
if "rom" in table:
    e = table["rom"]
else:
    raw_pals = []
out = {"title": a.title, "console": "snes", "crc32": crc, "size": len(rom_bytes),
       "tracedTiles": stats["tiles"], "explainedTiles": stats["explained"],
       "streams": streams, "rawTiles": raw, "rawPalettes": raw_pals}
json.dump(out, open(a.out, "w"), indent=1)
print(f"{a.title} [{crc}]: {stats['explained']}/{stats['tiles']} tiles explained, {len(streams)} streams, {len(raw)} raw runs")
