#!/usr/bin/env python3
"""A minimal encoder of Unity crunch files (DXT1 and DXT5, one level), for the test fixtures only: it
does not compress (every symbol of a model has a code of the same length, every distinct endpoint and
selector is a palette entry), but the file follows the format that drv_crunch.pas reads. The independent
check of the decoder is texture2ddecoder.unpack_unity_crunch (see verify_texdecode.py)."""


class Bits:
    def __init__(self):
        self.bits = []

    def put(self, v, n):
        self.bits.extend((v >> (n - 1 - i)) & 1 for i in range(n))

    def bytes(self):
        b = list(self.bits)
        while len(b) % 8:
            b.append(0)
        return bytes(int("".join(map(str, b[i:i + 8])), 2) for i in range(0, len(b), 8))


PROB = [17, 18, 19, 20, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15, 16]


def pow2(n):
    return 1 if n <= 1 else 1 << (n - 1).bit_length()


def model(bits, nsyms):
    """Sends a model of nsyms symbols (a power of two) that all have a code of the same length k (one
    symbol: 1 bit); the code of the symbol s is s on k bits. Returns k."""
    k = max(1, (nsyms - 1).bit_length())
    pos = PROB.index(k)
    bits.put(nsyms, 14)
    bits.put(pos + 1, 5)
    for i in range(pos + 1):
        bits.put(1 if i == pos else 0, 3)      # the code length k has the only code of the length code model: "0"
    for _ in range(nsyms):
        bits.put(0, 1)                         # every symbol has the length k
    return k


def encode(fmt, w, h, blocks):
    """fmt 0 = DXT1: blocks = [(color endpoints dword, color selectors dword)] row by row;
    fmt 2 = DXT5: blocks = [(alpha endpoints 16 bits, alpha selectors 48 bits, color endpoints, color selectors)]"""
    bx, by = (w + 3) // 4, (h + 3) // 4
    assert len(blocks) == bx * by
    px, py = (bx + 1) & ~1, (by + 1) & ~1
    dxt5 = fmt != 0
    cols = sorted({b[2] if dxt5 else b[0] for b in blocks})
    csel = sorted({b[3] if dxt5 else b[1] for b in blocks})
    aend = sorted({b[0] for b in blocks}) if dxt5 else []
    asel = sorted({b[1] for b in blocks}) if dxt5 else []

    # tables: reference model, then the delta and selector models of the color, then of the alpha
    t = Bits()
    model(t, 1)
    kce = model(t, pow2(len(cols)))
    ksel = model(t, pow2(len(csel)))
    if dxt5:
        kae = model(t, pow2(len(aend)))
        kas = model(t, pow2(len(asel)))
    tables = t.bytes()

    # color endpoints: 6 values (5, 6, 5, 5, 6, 5 bits) as deltas modulo their range
    p = Bits()
    model(p, 32)
    model(p, 64)
    prev = [0] * 6
    for v in cols:
        f = (v >> 11 & 31, v >> 5 & 63, v & 31, v >> 27 & 31, v >> 21 & 63, v >> 16 & 31)
        for i, size in enumerate((32, 64, 32, 32, 64, 32)):
            p.put((f[i] - prev[i]) & (size - 1), 5 if size == 32 else 6)
            prev[i] = f[i]
    cep = p.bytes()

    # color selectors: 8 symbols of 4 bits, xor coded; the decoder turns the 2 bits pairs (s1, s0) into
    # ((s ^ s << 1) & 0xAAAAAAAA) | (s >> 1 & 0x55555555)
    p = Bits()
    model(p, 16)
    S = 0
    for v in csel:
        target = 0
        for k in range(16):
            lo, hi = v >> (2 * k) & 1, v >> (2 * k + 1) & 1
            target |= (lo << (2 * k + 1)) | ((hi ^ lo) << (2 * k))
        for j in range(0, 32, 4):
            sym = ((target ^ S) >> j) & 15
            p.put(sym, 4)
            S ^= sym << j
    csp = p.bytes()

    aep = asp = b""
    if dxt5:
        p = Bits()
        model(p, 256)
        a = b = 0
        for v in aend:
            p.put(((v & 255) - a) & 255, 8)
            p.put(((v >> 8) - b) & 255, 8)
            a, b = v & 255, v >> 8
        aep = p.bytes()
        from_lin = [0, 2, 3, 4, 5, 6, 7, 1]
        to_lin = [from_lin.index(i) for i in range(8)]
        p = Bits()
        model(p, 64)
        lin = [0, 0]
        for v in asel:
            for half, g in ((v & 0xFFFFFF, 0), (v >> 24, 1)):
                for j in range(0, 24, 6):
                    pair = (half >> j) & 0x3F
                    want = to_lin[pair & 7] | (to_lin[pair >> 3] << 3)
                    sym = ((lin[g] >> j) & 0x3F) ^ want
                    lin[g] ^= sym << j
                    p.put(sym, 6)
        asp = p.bytes()

    # the level: for every block of the grid (padded to even sizes): the reference group on the first of
    # each 2x2 (0 = new endpoint everywhere), the endpoint deltas, the selector indexes
    cmap = {v: i for i, v in enumerate(cols)}
    smap = {v: i for i, v in enumerate(csel)}
    amap = {v: i for i, v in enumerate(aend)}
    asmap = {v: i for i, v in enumerate(asel)}
    L = Bits()
    ci = ai = 0
    for y in range(py):
        for x in range(px):
            if y % 2 == 0 and x % 2 == 0:
                L.put(0, 1)
            vis = x < bx and y < by
            b = blocks[y * bx + x] if vis else None
            if vis:
                ni = cmap[b[2] if dxt5 else b[0]]
                L.put((ni - ci) % len(cols), kce)
                ci = ni
                if dxt5:
                    na = amap[b[0]]
                    L.put((na - ai) % len(aend), kae)
                    ai = na
                L.put(smap[b[3] if dxt5 else b[1]], ksel)
                if dxt5:
                    L.put(asmap[b[1]], kas)
            else:
                L.put(0, kce)
                if dxt5:
                    L.put(0, kae)
                L.put(0, ksel)
                if dxt5:
                    L.put(0, kas)
    level = L.bytes()

    hdr_size = 74
    streams = [tables, cep, csp, aep, asp]
    pos, offs = hdr_size, []
    for s in streams:
        offs.append(pos)
        pos += len(s)
    total = pos + len(level)

    def pal(o, size, num):
        return o.to_bytes(3, "big") + size.to_bytes(3, "big") + num.to_bytes(2, "big")

    head = b"Hx" + hdr_size.to_bytes(2, "big") + bytes(2) + total.to_bytes(4, "big") + bytes(2)
    head += w.to_bytes(2, "big") + h.to_bytes(2, "big") + bytes([1, 1, fmt]) + bytes(2) + bytes(12)
    head += pal(offs[1], len(cep), len(cols)) + pal(offs[2], len(csp), len(csel))
    head += pal(offs[3], len(aep), len(aend)) if dxt5 else bytes(8)
    head += pal(offs[4], len(asp), len(asel)) if dxt5 else bytes(8)
    head += len(tables).to_bytes(2, "big") + offs[0].to_bytes(3, "big") + pos.to_bytes(4, "big")
    assert len(head) == hdr_size
    return head + b"".join(streams) + level
