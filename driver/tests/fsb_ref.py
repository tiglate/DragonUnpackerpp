"""Independent reader (python) of the FSB5 files and FMOD banks, to check drv_fmt_fmod.pas: what every sound becomes
(WAV for PCM, an MPEG stream without the padding of FMOD, an Ogg Vorbis file, or the raw data).
expected(path) -> [(name, offset, size, data)]; the entries are in the order of the headers."""
import os, struct, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
CODEC = ["none", "pcm8", "pcm16", "pcm24", "pcm32", "float", "gcadpcm", "imaadpcm", "vag", "hevag", "xma", "mpeg", "celt", "at9", "xwma", "vorbis"]
FQ = [4000, 8000, 11000, 11025, 16000, 22050, 24000, 32000, 44100, 48000]


def books():
    raw = zlib.decompress(open(os.path.join(HERE, "fsb_codebooks.zlib"), "rb").read())
    n, = struct.unpack("<I", raw[:4])
    base = 4 + 12 * n
    out = {}
    for i in range(n):
        crc, off, size = struct.unpack("<III", raw[4 + 12 * i:16 + 12 * i])
        out[crc] = raw[base + off:base + off + size]
    return out


class BR:
    def __init__(s, b): s.b = b; s.p = 0
    def read(s, n):
        v = 0
        for i in range(n):
            byte = s.p >> 3
            if byte >= len(s.b): raise EOFError
            v |= ((s.b[byte] >> (s.p & 7)) & 1) << i; s.p += 1
        return v


def ilog(n):
    r = 0
    while n: r += 1; n >>= 1
    return r


def vorbis_modes(b, channels):
    """the blockflags of the modes of a setup header (full parse of the header); None when it is not valid"""
    try:
        r = BR(b)
        assert r.read(8) == 5 and bytes(r.read(8) for _ in range(6)) == b"vorbis"
        for _ in range(r.read(8) + 1):
            assert r.read(24) == 0x564342
            dim = r.read(16); ent = r.read(24)
            if r.read(1):
                r.read(5); i = 0
                while i < ent: i += r.read(ilog(ent - i))
            else:
                sparse = r.read(1)
                for _ in range(ent):
                    if sparse:
                        if r.read(1): r.read(5)
                    else: r.read(5)
            lt = r.read(4)
            if lt in (1, 2):
                r.read(32); r.read(32); vb = r.read(4) + 1; r.read(1)
                if lt == 1:
                    v = 0
                    while (v + 1) ** dim <= ent: v += 1
                else: v = ent * dim
                for _ in range(v): r.read(vb)
            else: assert lt == 0
        for _ in range(r.read(6) + 1): assert r.read(16) == 0
        for _ in range(r.read(6) + 1):
            t = r.read(16)
            if t == 1:
                parts = r.read(5); cl = [r.read(4) for _ in range(parts)]; dims = {}
                for c in range(max(cl) + 1 if cl else 1):
                    dims[c] = r.read(3) + 1; sub = r.read(2)
                    if sub: r.read(8)
                    for _ in range(1 << sub): r.read(8)
                r.read(2); rb = r.read(4)
                for p in range(parts):
                    for _ in range(dims[cl[p]]): r.read(rb)
            else:
                assert t == 0
                r.read(8); r.read(16); r.read(16); r.read(6); r.read(8)
                for _ in range(r.read(4) + 1): r.read(8)
        for _ in range(r.read(6) + 1):
            assert r.read(16) <= 2
            r.read(24); r.read(24); r.read(24); nc = r.read(6) + 1; r.read(8)
            casc = []
            for _ in range(nc):
                lo = r.read(3); hi = r.read(5) if r.read(1) else 0
                casc.append(hi * 8 + lo)
            for c in casc:
                for j in range(8):
                    if c & (1 << j): r.read(8)
        for _ in range(r.read(6) + 1):
            assert r.read(16) == 0
            subs = r.read(4) + 1 if r.read(1) else 1
            if r.read(1):
                for _ in range(r.read(8) + 1): r.read(ilog(channels - 1)); r.read(ilog(channels - 1))
            assert r.read(2) == 0
            if subs > 1:
                for _ in range(channels): r.read(4)
            for _ in range(subs): r.read(8); r.read(8); r.read(8)
        modes = []
        for _ in range(r.read(6) + 1):
            bf = r.read(1); assert r.read(16) == 0 and r.read(16) == 0; r.read(8); modes.append(bf)
        assert r.read(1) == 1
        return modes
    except Exception:
        return None


_CT = []
for _i in range(256):
    _r = _i << 24
    for _ in range(8): _r = ((_r << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if _r & 0x80000000 else (_r << 1) & 0xFFFFFFFF
    _CT.append(_r)


def ogg_crc(b):
    c = 0
    for x in b: c = ((c << 8) & 0xFFFFFFFF) ^ _CT[((c >> 24) & 255) ^ x]
    return c


def page(seq, granule, flags, segs, body):
    h = b"OggS" + bytes([0, flags]) + struct.pack("<qIII", granule, 1, seq, 0) + bytes([len(segs)]) + bytes(segs)
    pg = h + body
    return pg[:22] + struct.pack("<I", ogg_crc(pg)) + pg[26:]


def lace(n):
    return [255] * (n // 255) + [n % 255]


def to_ogg(data, rate, ch, samples, setup, modes):
    ident = b"\x01vorbis" + struct.pack("<IBIiiiB", 0, ch, rate, 0, 0, 0, 0xB8) + b"\x01"
    comment = b"\x03vorbis" + struct.pack("<II", 0, 0) + b"\x01"
    pk, pos = [], 0
    while pos + 2 <= len(data):
        n, = struct.unpack("<H", data[pos:pos + 2]); pos += 2
        if n == 0 or n == 0xFFFF or pos + n > len(data): break
        pk.append(data[pos:pos + n]); pos += n
    mbits = ilog(len(modes) - 1)
    out = [page(0, 0, 2, lace(len(ident)), ident), page(1, 0, 0, lace(len(comment)) + lace(len(setup)), comment + setup)]
    pages, segs, body, total, prev, gran = [], [], b"", 0, None, 0
    for p in pk:
        mode = (p[0] >> 1) & ((1 << mbits) - 1)
        bs = 2048 if modes[mode] else 256
        if prev is not None: total += prev // 4 + bs // 4
        prev = bs
        s = lace(len(p))
        if len(segs) + len(s) > 255:
            pages.append((segs, body, min(gran, samples))); segs, body = [], b""
        segs += s; body += p; gran = total
    pages.append((segs, body, min(gran, samples)))
    seq = 2
    for k, (s, b, g) in enumerate(pages):
        out.append(page(seq, g, 4 if k == len(pages) - 1 else 0, s, b)); seq += 1
    return b"".join(out)


def mpeg_len(h):
    if h >> 21 != 0x7FF: return 0, 0
    ver, lay, bi, si, pad = h >> 19 & 3, 4 - (h >> 17 & 3), h >> 12 & 15, h >> 10 & 3, h >> 9 & 1
    if ver == 1 or (h >> 17 & 3) == 0 or bi in (0, 15) or si == 3: return 0, 0
    BR = [[0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448], [0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384],
          [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320], [0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256],
          [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160]]
    sr = [44100, 48000, 32000][si] // (1 if ver == 3 else 2) // (2 if ver == 0 else 1)
    row = lay - 1 if ver == 3 else (3 if lay == 1 else 4)
    br = BR[row][bi] * 1000
    if lay == 1: n = (12 * br // sr + pad) * 4
    elif lay == 2 or ver == 3: n = 144 * br // sr + pad
    else: n = 72 * br // sr + pad
    return n, lay


def mpeg_repack(data, pad):
    out, pos = b"", 0
    while pos + 4 <= len(data):
        n, lay = mpeg_len(struct.unpack(">I", data[pos:pos + 4])[0])
        if n < 4 or pos + n > len(data): break
        out += data[pos:pos + n]
        pos += n + (pad - n % pad if pad and (lay == 3 or pad == 16) and n % pad else 0)
    return out


def expected(path):
    d = open(path, "rb").read()
    base = 0
    if d[:4] == b"RIFF":
        i = 12
        while i + 8 <= len(d):
            sz, = struct.unpack("<I", d[i + 4:i + 8])
            if d[i:i + 4] == b"SND ":
                j = d[i + 8:i + 72].find(b"FSB5")
                base = i + 8 + j if j >= 0 else 0
                break
            i += 8 + sz + (sz & 1)
        if not base: return []
    ver, ns, shs, nts, ds, mode = struct.unpack("<6I", d[base + 4:base + 28])
    hl = 60 if ver == 1 else 56
    q, ents = base + hl, []
    for i in range(ns):
        bits, = struct.unpack("<Q", d[q:q + 8]); q += 8
        e = {"off": ((bits >> 7) & 0x7FFFFFF) * 32, "ch": {0: 1, 1: 2, 2: 6, 3: 8}[bits >> 5 & 3], "rate": FQ[bits >> 1 & 15] if bits >> 1 & 15 < 10 else 44100,
             "samples": bits >> 34, "crc": None, "layers": False}
        ex = bits & 1
        while ex:
            c, = struct.unpack("<I", d[q:q + 4]); q += 4; ex = c & 1; sz = c >> 1 & 0xFFFFFF; ty = c >> 25
            if ty == 2: e["rate"], = struct.unpack("<I", d[q:q + 4])
            if ty == 1: e["ch"] = d[q]
            if ty == 11: e["crc"], = struct.unpack("<I", d[q:q + 4])
            if ty == 14: e["layers"] = True
            q += sz
        ents.append(e)
    ds0, nt = base + hl + shs + nts, base + hl + shs
    allo = sorted({e["off"] for e in ents} | {ds})
    out, seen = [], set()
    bk = books()
    for i, e in enumerate(ents):
        nxt = [o for o in allo if o > e["off"]][0]
        data = d[ds0 + e["off"]:ds0 + nxt]
        off = ds0 + e["off"]
        nm = ""
        if nts:
            no, = struct.unpack("<I", d[nt + 4 * i:nt + 4 * i + 4])
            nm = d[nt + no:d.index(b"\0", nt + no)].decode("latin-1")
        nm = nm.replace("/", "_").replace("\\", "_") if nm else "sample_%05d" % i
        ext = ".bin"
        if 1 <= mode <= 5:
            bps = {1: 1, 2: 2, 3: 3, 4: 4, 5: 4}[mode]
            need = bps * e["ch"] * e["samples"]
            if e["samples"] > 0 and need <= len(data): data = data[:need]
            ch, bits = e["ch"], bps * 8
            data = (b"RIFF" + struct.pack("<I", len(data) + 36) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 3 if mode == 5 else 1, ch, e["rate"], e["rate"] * ch * bps, ch * bps, bits) + b"data" + struct.pack("<I", len(data)) + data)
            ext = ".wav"
        elif mode == 11:
            ext = ".mp3"
            if e["ch"] > 2: ext = ".mpeg.bin"
            else:
                r = mpeg_repack(data, 4)
                if r: data = r
        elif mode == 15:
            ext = ".vorbis.bin"
            if e["crc"] in bk and not e["layers"]:
                modes = vorbis_modes(bk[e["crc"]], e["ch"])
                if modes:
                    data = to_ogg(data, e["rate"], e["ch"], e["samples"], bk[e["crc"]], modes); ext = ".ogg"
        else:
            ext = "." + CODEC[mode] + ".bin"
        if (nm + ext).lower() in seen: nm += "_%d" % i
        seen.add((nm + ext).lower())
        out.append((nm + ext, off, len(data), data))
    return out
