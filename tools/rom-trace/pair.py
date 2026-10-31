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
ap.add_argument("--max-run-palettes", type=int, default=2)
ap.add_argument("--probe", choices=["dkc", "lz5"], nargs="+", default=[], help="also look for streams of a codec that the driver does not scan for: every ROM offset is tried and a stream counts when its tiles are on the screen of the trace")
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
index4, index2, index8 = {}, {}, {}   # tile -> (stream, tile number, bpp)
for src, b in blobs.items():
    for size, conv, bpp in ((32, bytes, 4), (24, to4, 3)):
        for k in range(len(b) // size):
            t = conv(b[k * size:(k + 1) * size])
            if any(t): index4.setdefault(t, (src, k, bpp))
    for k in range(len(b) // 16):
        t = b[k * 16:(k + 1) * 16]
        if any(t): index2.setdefault(t, (src, k, 2))
    for k in range(len(b) // 64):   # Mode 7: 8 bits per pixel, one byte per pixel
        t = b[k * 64:(k + 1) * 64]
        if any(t): index8.setdefault(t, (src, k, 8))
rom_cache = {}
def in_rom(t):
    if t not in rom_cache: rom_cache[t] = rom_bytes.find(t)
    return rom_cache[t]

def color(w):
    r, g, b = w & 31, (w >> 5) & 31, (w >> 10) & 31
    return "%02x%02x%02x" % (r * 255 // 31, g * 255 // 31, b * 255 // 31)

recs = []
for l in open(a.trace):
    try: recs.append(json.loads(l))
    except ValueError: pass   # the last line of a trace that was cut
vrams = {r["frame"]: bytes.fromhex(r["data"]) for r in recs if r["t"] == "vram"}
cgrams = [(r["frame"], bytes.fromhex(r["data"])) for r in recs if r["t"] == "cgram"]
def cgram_at(frame):
    best = None
    for f, d in cgrams:
        if f <= frame: best = d
    return best

def dkc_decode(rom, table, maxout=0x8000):
    """The Donkey Kong Country compression (see DecodeDkc in the driver), None when the data is not a stream"""
    p, n, out = table + 128, len(rom), bytearray()
    while p < n:
        b = rom[p]; p += 1
        if b == 0: return bytes(out)
        c, k = b & 0xC0, b & 0x3F
        if c == 0:
            if p + b > n: return None
            out += rom[p:p + b]; p += b
        elif c == 0x40:
            if k == 0 or p >= n: return None
            out += bytes([rom[p]]) * k; p += 1
        elif c == 0x80:
            if k == 0 or p + 2 > n: return None
            off = rom[p] | rom[p + 1] << 8; p += 2
            if off >= len(out): return None
            for i in range(k): out.append(out[off + i])
        else:
            out += rom[table + k * 2:table + k * 2 + 2]
        if len(out) > maxout: return None
    return None

def m7pack(t):
    """A Mode 7 tile (64 pixel bytes) that the game made from 4 bits pixels plus a base: (base, 32 packed bytes, low
    nibble first), else None"""
    base = min(t) & 0xF0
    if max(t) - base > 15: return None
    return base, bytes((t[2 * i] - base) | ((t[2 * i + 1] - base) << 4) for i in range(32))

def lz5_decode(rom, start, maxout=0x10000):
    """The Nintendo LZ with the two extra copy commands (Super Metroid, Super Mario Kart), None when not a stream"""
    p, n, out = start, len(rom), bytearray()
    while p < n:
        h = rom[p]; p += 1
        if h == 0xFF: return bytes(out)
        cmd, ln = h >> 5, (h & 31) + 1
        if cmd == 7:
            if p >= n: return None
            cmd, ln = (h >> 2) & 7, ((h & 3) << 8 | rom[p]) + 1; p += 1
        if cmd == 0:
            if p + ln > n: return None
            out += rom[p:p + ln]; p += ln
        elif cmd == 1:
            if p >= n: return None
            out += bytes([rom[p]]) * ln; p += 1
        elif cmd == 2:
            if p + 2 > n: return None
            out += bytes(rom[p + (i & 1)] for i in range(ln)); p += 2
        elif cmd == 3:
            if p >= n: return None
            out += bytes((rom[p] + i) & 255 for i in range(ln)); p += 1
        elif cmd in (4, 5):
            if p + 2 > n: return None
            f = rom[p] | rom[p + 1] << 8; p += 2
            if f >= len(out): return None
            for i in range(ln): out.append(out[f + i] ^ (0xFF if cmd == 5 else 0))
        elif cmd == 6:
            if p >= n: return None
            back = rom[p]; p += 1
            if back == 0 or back > len(out): return None
            f = len(out) - back
            for i in range(ln): out.append(out[f + i])
        else:
            return None
        if len(out) > maxout: return None
    return None

for codec in a.probe:
    seen32, seen64 = set(), set()
    for v in vrams.values():
        for i in range(0, len(v) - 31, 32):
            t = v[i:i + 32]
            if any(t): seen32.add(t)
        chars = v[1:32768:2]   # Mode 7 tiles: the high bytes of the words
        for i in range(0, len(chars) - 63, 64):
            t = chars[i:i + 64]
            if any(t):
                seen64.add(t)
                pk = m7pack(t)
                if pk: seen32.add(pk[1])
    decode = dkc_decode if codec == "dkc" else lz5_decode
    cands = []
    for t0 in range(len(rom_bytes) - (200 if codec == "dkc" else 8)):
        out = decode(rom_bytes, t0)
        if out is None or len(out) < 64: continue
        b32 = [out[i:i + 32] for i in range(0, len(out) - 31, 32)]
        b64 = [out[i:i + 64] for i in range(0, len(out) - 63, 64)]
        h32 = sum(1 for b in b32 if b in seen32)
        h64 = sum(1 for b in b64 if b in seen64)
        hits, blocks = (h32, b32) if h32 * 2 >= h64 * 1 else (h64, b64)
        if hits >= 8 and hits >= 0.3 * len(blocks):
            cands.append((hits, t0, out, blocks))
    cands.sort(key=lambda c: (-c[0], c[1]))   # the best explained first: a start a few bytes off decodes much the same
    for hits, t0, out, blocks in cands:
        name = "%s_%06X" % (codec, t0)
        blobs[name] = out
        for k, b in enumerate(blocks):
            if any(b): (index4 if len(b) == 32 else index8).setdefault(b, (name, k, 4 if len(b) == 32 else 8))
    print("probe %s: %d candidates" % (codec, len(cands)))

class Entry:
    def __init__(self): self.palettes = {}; self.tiles = set(); self.kinds = set(); self.bpp = set(); self.offsets = collections.defaultdict(collections.Counter)
table = collections.defaultdict(Entry)   # stream name, or "rom"
stats = collections.Counter()
for r in recs:
    if r["t"] not in ("bg", "oam") or r["frame"] not in vrams: continue
    vram, cg = vrams[r["frame"]], cgram_at(r["frame"])
    if cg is None: continue
    if r["t"] == "bg" and r["mode"] == 7:
        if r["bg"] != 1: continue
        # Mode 7: the map is the low bytes of the first 16 K words of VRAM, the 8 bit tiles are their high bytes
        chars = vram[1:32768:2]
        for tile in sorted(set(vram[0:32768:2])):
            t = chars[tile * 64:(tile + 1) * 64]
            if not any(t): continue
            stats["tiles"] += 1
            hit, base = index8.get(t), 0
            if hit: src, tileno, bpp = hit; off = None
            else:
                off = in_rom(t)
                if off >= 0: src, bpp = "rom", 8
                else:
                    pk = m7pack(t)   # made from 4 bits pixels (a converter in the game): look the packed form up
                    if not pk: continue
                    base, packed = pk
                    hit = index4.get(packed)
                    if hit: src, tileno, bpp = hit[0], hit[1], 5; off = None
                    else:
                        off = in_rom(packed)
                        if off < 0: continue
                        src, bpp = "rom", 5
            stats["explained"] += 1
            e = table[src]
            n = 16 if bpp == 5 else 256
            colors = [int.from_bytes(cg[(base + c) * 2:(base + c) * 2 + 2], "little") for c in range(n)]
            key = " ".join(color(c) for c in colors)
            p = e.palettes.setdefault(key, {"count": 0, "romOffset": None})
            p["count"] += 1
            e.kinds.add("mode7"); e.bpp.add(bpp)
            if off is not None: e.offsets[(off, 32 if bpp == 5 else 64, bpp)][key] += 1
            else: e.tiles.add(tileno)
        continue
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
        if off is not None: e.offsets[(off, size, 4 if size == 32 else 2)][key] += 1
        else: e.tiles.add(tileno)

streams, raw = [], []
for src, e in sorted(table.items()):
    pals = [{"colors": k.split(), "romOffset": v["romOffset"], "count": v["count"]}
            for k, v in sorted(e.palettes.items(), key=lambda kv: -kv[1]["count"])[:a.max_palettes]]
    if src == "rom":
        runs = []   # tiles found at ROM offsets, merged into runs of consecutive tiles of one format, with their palettes
        for off, size, bpp in sorted(e.offsets, key=lambda o: (o[2], o[0])):
            used = e.offsets[(off, size, bpp)]
            if runs and runs[-1][2] == bpp and off == runs[-1][0] + runs[-1][1]:
                runs[-1][1] += size; runs[-1][3].update(used)
            else: runs.append([off, size, bpp, collections.Counter(used)])
        raw = [{"offset": o, "length": n, "bpp": b,
                "palettes": [{"colors": k.split(), "count": c} for k, c in pc.most_common(a.max_run_palettes)]} for o, n, b, pc in runs]
        raw_pals = pals
        continue
    m = re.match(r"(\w+?)_([0-9A-Fa-f]+)$", src)
    if m.group(1) in ("dkc", "lz5") and len(e.tiles) < 8: continue   # a probed stream with a few tiles is a coincidence
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
