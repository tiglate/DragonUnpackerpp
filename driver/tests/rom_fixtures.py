"""Synthetic console ROMs (NES, SNES, Mega Drive plain and Super Magic Drive) with tiles of
known pixels, shared by make_fixtures.py (fuzzing) and verify_roms.py (content checks)."""
import random, struct

TILES = 64        # the first tiles carry a pattern, the rest is zero


def pattern(n):
    r = random.Random(n)
    return bytes(r.randrange(256) for _ in range(n))


def nes():
    chr_ = pattern(8192)
    return b"NES\x1a" + bytes([1, 1, 0x01, 0x10]) + bytes(8) + pattern(16384) + chr_, chr_


def snes(copier=False):
    rom = bytearray(pattern(32768))
    h = 0x7FC0
    rom[h:h + 21] = b"TEST ROM".ljust(21)
    rom[h + 0x15] = 0x20          # LoROM, SlowROM
    rom[h + 0x17] = 5             # 32 KB
    rom[h + 0x1C:h + 0x20] = struct.pack("<HH", 0xFFFF ^ 0x1234, 0x1234)
    rom[h + 0x3C:h + 0x3E] = struct.pack("<H", 0x8000)
    return (bytes(512) if copier else b"") + bytes(rom), bytes(rom)


def _md_rom():
    rom = bytearray(pattern(32768))
    rom[0x100:0x200] = (b"SEGA MEGA DRIVE " + b"(C)TEST 2024.JAN" + b"TEST GAME".ljust(48) + b"TEST GAME".ljust(48) +
                        b"GM 00000000-00".ljust(14) + b"\x12\x34" + b"J".ljust(16) +
                        struct.pack(">IIII", 0, 0x7FFF, 0xFF0000, 0xFFFFFF) + bytes(0x40) + b"JUE").ljust(0x100, b" ")
    return bytes(rom)


def megadrive():
    rom = _md_rom()
    return rom, rom


def smd():
    rom = _md_rom()
    out = bytearray(bytes([len(rom) // 16384, 3]) + bytes(6) + b"\xaa\xbb" + bytes(502))
    for b in range(0, len(rom), 16384):
        blk = rom[b:b + 16384]
        out += blk[1::2] + blk[0::2]       # odd bytes first
    return bytes(out), rom


def nemesis_encode(data, xor=False):
    """Simple Nemesis encoder: 4 bits codes for the nibbles 0-13 (runs of 1), the inline escape
    111111 + run + nibble for runs and for the nibbles 14 and 15"""
    assert len(data) % 32 == 0
    rows = [int.from_bytes(data[i:i + 4], "big") for i in range(0, len(data), 4)]
    if xor:
        rows = [r ^ (rows[i - 1] if i else 0) for i, r in enumerate(rows)]
    nibbles = [(r >> (28 - 4 * k)) & 15 for r in rows for k in range(8)]
    head = struct.pack(">H", (0x8000 if xor else 0) | (len(data) // 32))
    table = b""
    for n in range(14):
        table += bytes([0x80 | n, 0x04, n])
    table += b"\xff"
    bits = ""
    i = 0
    while i < len(nibbles):
        n, run = nibbles[i], 1
        while i + run < len(nibbles) and nibbles[i + run] == n and run < 8:
            run += 1
        if run == 1 and n < 14:
            bits += format(n, "04b")
        else:
            bits += "111111" + format(run - 1, "03b") + format(n, "04b")
        i += run
    bits += "0" * (-len(bits) % 8)
    return head + table + bytes(int(bits[k:k + 8], 2) for k in range(0, len(bits), 8))


def kosinski_encode(data):
    """Greedy Kosinski encoder; the next description word is placed as soon as the last bit of
    the previous one is used, as the format wants"""
    out = bytearray()
    state = {"bits": [], "slot": 0}
    out += b"\0\0"

    def put(bit):
        state["bits"].append(bit)
        if len(state["bits"]) == 16:
            word = sum(b << k for k, b in enumerate(state["bits"]))
            out[state["slot"]:state["slot"] + 2] = struct.pack("<H", word)
            state["bits"] = []
            state["slot"] = len(out)
            out.extend(b"\0\0")

    i = 0
    while i < len(data):
        best = (0, 0)
        for dist in range(1, min(i, 8192) + 1):
            n = 0
            while i + n < len(data) and n < 255 and data[i + n - dist] == data[i + n] if i + n - dist >= 0 else False:
                n += 1
            if n > best[0]:
                best = (n, dist)
        n, dist = best
        if n >= 2 and dist <= 256 and n <= 5:
            put(0); put(0); put(((n - 2) >> 1) & 1); put((n - 2) & 1)
            out.append(256 - dist)
            i += n
        elif n >= 3:
            n = min(n, 255)
            off = 0x2000 - dist
            lo, hi = off & 0xFF, (off >> 5) & 0xF8
            put(0); put(1)
            if n <= 9:
                out += bytes([lo, hi | (n - 2)])
            else:
                out += bytes([lo, hi, n - 1])
            i += n
        else:
            put(1)
            out.append(data[i])
            i += 1
    put(0); put(1)
    out += bytes([0, 0xF0, 0])
    if state["bits"]:
        word = sum(b << k for k, b in enumerate(state["bits"]))
        out[state["slot"]:state["slot"] + 2] = struct.pack("<H", word)
    return bytes(out)


def art_data(tiles, seed):
    """Tile data with the look of art: zero nibbles, runs and a few colors"""
    r = random.Random(seed)
    out = bytearray()
    for _ in range(tiles * 8):
        row = 0
        for _ in range(8):
            row = (row << 4) | (0 if r.random() < 0.5 else r.choice([1, 2, 3, 5, 14, 15]))
        out += row.to_bytes(4, "big")
    return bytes(out)


def kos_data(size, seed):
    r = random.Random(seed)
    out = bytearray()
    while len(out) < size:
        if out and r.random() < 0.5:
            d = r.randint(1, min(len(out), 3000))
            for _ in range(r.randint(2, 40)):
                out.append(out[-d])
        else:
            out.append(r.randrange(256))
    return bytes(out[:size])


def md_compressed():
    """A Mega Drive ROM holding Nemesis (plain and XOR) and Kosinski streams; returns the ROM and
    [(codec, offset, data)]"""
    rom = bytearray(_md_rom())
    for i in range(0x400, len(rom)):
        rom[i] = 0
    parts = [("nemesis", art_data(40, 1), nemesis_encode(art_data(40, 1))),
             ("nemesis", art_data(33, 2), nemesis_encode(art_data(33, 2), True)),
             ("kosinski", kos_data(3000, 3), kosinski_encode(kos_data(3000, 3))),
             ("kosinski", pattern(40) + pattern(40)[:30] + pattern(24), kosinski_encode(pattern(40) + pattern(40)[:30] + pattern(24)))]
    at, found = 0x400, []
    for codec, data, packed in parts:
        rom[at:at + len(packed)] = packed
        found.append((codec, at, data))
        at += (len(packed) + 0x20) & ~1
    # a table of the addresses of the streams, as the games have (the scanner starts there)
    for k, (codec, offset, data) in enumerate(found):
        rom[0x300 + 4 * k:0x304 + 4 * k] = struct.pack(">I", offset)
    return bytes(rom), found


def all_fixtures():
    return [("test.nes", nes()[0]), ("test.sfc", snes()[0]), ("copier.smc", snes(True)[0]), ("test.md", megadrive()[0]), ("packed.md", md_compressed()[0]),
            ("test.smd", smd()[0])]
