"""Synthetic console ROMs (NES, SNES, Mega Drive plain and Super Magic Drive) with tiles of
known pixels, shared by make_fixtures.py (fuzzing) and verify_roms.py (content checks)."""
import math, random, struct

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
             ("kosinski", kos_data(3008, 3), kosinski_encode(kos_data(3008, 3))),
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


def lz2_encode(data, variant="little"):
    """Greedy Nintendo LZ encoder (commands 0-4, the long form for lengths over 32); the "metroid" variant
    also uses 5 (copy with the bits flipped) and 6 (copy from a byte offset back)"""
    out = bytearray()

    def emit(cmd, length, payload):
        if length <= 32:
            out.append(cmd << 5 | (length - 1))
        else:
            out.extend([0xE0 | cmd << 2 | (length - 1) >> 8, (length - 1) & 255])
        out.extend(payload)

    literal = bytearray()

    def flush():
        while literal:
            chunk = literal[:1024]
            del literal[:1024]
            emit(0, len(chunk), chunk)

    i = 0
    while i < len(data):
        run = 1
        while i + run < len(data) and data[i + run] == data[i] and run < 1024:
            run += 1
        word = 2
        while i + word < len(data) and data[i + word] == data[i + (word & 1)] and word < 1024:
            word += 1
        inc = 1
        while i + inc < len(data) and data[i + inc] == (data[i] + inc) & 255 and inc < 1024:
            inc += 1
        best = (0, 0)
        for start in range(0, i):
            n = 0
            while i + n < len(data) and n < 1024 and data[start + n] == data[i + n]:
                n += 1
            if n > best[0]:
                best = (n, start)
        choices = [(run, 1, bytes([data[i]])), (word, 2, bytes(data[i:i + 2])), (inc, 3, bytes([data[i]])),
                   (best[0], 4, struct.pack(">H" if variant == "big" else "<H", best[1]))]
        if variant == "metroid":
            if best[0] and i - best[1] <= 255:       # a copy from a byte offset back
                choices.append((best[0], 6, bytes([i - best[1]])))
            flipped = (0, 0)
            for start in range(0, i):
                n = 0
                while i + n < len(data) and n < 1024 and data[start + n] ^ 0xFF == data[i + n]:
                    n += 1
                if n > flipped[0]:
                    flipped = (n, start)
            choices.append((flipped[0], 5, struct.pack("<H", flipped[1])))
        n, cmd, payload = max(choices, key=lambda c: (c[0], -len(c[2])))
        if n >= 4 and not (cmd == 2 and n < 6):
            flush()
            emit(cmd, n, payload)
            i += n
        else:
            literal.append(data[i])
            i += 1
    flush()
    return bytes(out) + b"\xff"


def snes_art(size, seed):
    """Tile-like bytes: zero runs, repeated tiles, a few random ones"""
    r = random.Random(seed)
    out = bytearray()
    while len(out) < size:
        k = r.random()
        if k < 0.3:
            out += bytes(r.choice([8, 16, 32]))
        elif k < 0.5 and len(out) > 64:
            at = r.randrange(0, len(out) - 32)
            out += out[at:at + 32]
        elif k < 0.6 and len(out) > 64:           # the same bytes flipped (command 5 of Super Metroid)
            at = r.randrange(0, len(out) - 32)
            out += bytes(b ^ 0xFF for b in out[at:at + 32])
        else:
            out += bytes(r.choice([0, 0, 0x18, 0x3C, 0x7E, 0xFF, r.randrange(256)]) for _ in range(16))
    return bytes(out[:size])


def lzss_encode(data, counts_itself=True):
    """Greedy encoder of the LZSS of Final Fantasy VI: a 16 bits size, flag bytes from the lowest bit (1: literal,
    0: a reference of two bytes), a ring of 2048 bytes written from 0x7DE, references of 3 to 34 bytes"""
    body = bytearray()
    i = 0
    flags_at = None
    count = 0
    while i < len(data):
        if count == 0:
            flags_at = len(body)
            body.append(0)
        best_len, best_from = 0, 0
        for j in range(max(0, i - 2047), i):
            n = 0
            while n < 34 and i + n < len(data) and data[j + n] == data[i + n]:
                n += 1
            if n > best_len:
                best_len, best_from = n, j
        if best_len >= 3:
            pos = (0x7DE + best_from) & 2047
            body += bytes([pos & 255, (pos >> 8) | ((best_len - 3) << 3)])
            i += best_len
        else:
            body[flags_at] |= 1 << count
            body.append(data[i])
            i += 1
        count = (count + 1) % 8
    size = len(body) + (2 if counts_itself else 0)
    return struct.pack("<H", size) + bytes(body)


def snes_lzss(counts_itself=True):
    """A LoROM ROM with a table of 10 addresses (3 bytes each, at 0x800) of blocks stored one after the other from
    0x1000, as the games do; returns the ROM and [(offset, data)]"""
    rom = bytearray(snes()[1])
    for i in range(0x1000, 0x7FA0):
        rom[i] = 0
    datas = [snes_art(1024 + 32 * k, 60 + k) for k in range(9)]
    at, found, pointers = 0x1000, [], []
    for data in datas:
        packed = lzss_encode(data, counts_itself)
        rom[at:at + len(packed)] = packed
        found.append((at, data))
        pointers.append(at)
        at += len(packed)
    pointers.append(at)          # the end of the last one, as a table closes
    for k, offset in enumerate(pointers):
        rom[0x800 + 3 * k:0x803 + 3 * k] = struct.pack("<I", 0x80_8000 + offset)[:3]
    return bytes(rom), found


def snes_compressed(variant="little"):
    """A LoROM ROM with Nintendo LZ streams (eight abutting ones and a big one) and a table of long
    addresses pointing at them; returns the ROM (no copier header) and [(offset, data)]"""
    rom = bytearray(snes()[1])
    for i in range(0x1000, 0x7FA0):
        rom[i] = 0
    datas = [snes_art(1536, 11 + k) for k in range(8)] + [snes_art(3072, 40)]
    at, found = 0x1000, []
    for k, data in enumerate(datas):
        packed = lz2_encode(data, variant)
        rom[at:at + len(packed)] = packed
        found.append((at, data))
        at += len(packed) + (0x40 if k == len(datas) - 2 else 0)
    for k, (offset, data) in enumerate(found):
        rom[0x800 + 3 * k:0x803 + 3 * k] = struct.pack("<I", 0x80_8000 + offset)[:3]
    return bytes(rom), found


def brr_decode(blocks):
    """Reference BRR decoder written from the description of the S-DSP (15 bits samples)"""
    out, p1, p2 = [], 0, 0
    for b in range(0, len(blocks), 9):
        rng, filt = blocks[b] >> 4, (blocks[b] >> 2) & 3
        for i in range(16):
            n = blocks[b + 1 + i // 2]
            n = (n & 15) if i & 1 else n >> 4
            n = n - 16 if n >= 8 else n
            x = (n << rng) >> 1
            if filt == 1:
                x += p1 + ((-p1) >> 4)
            elif filt == 2:
                x += 2 * p1 + ((-3 * p1) >> 5) - p2 + (p2 >> 4)
            elif filt == 3:
                x += 2 * p1 + ((-13 * p1) >> 6) - p2 + ((3 * p2) >> 4)
            x = max(-32768, min(32767, x))
            y = ((x * 2 + 32768) & 0xFFFF) - 32768        # wrapped as a signed 16 bits
            p2, p1 = p1, y >> 1
            out.append(y)
    return out


def brr_encode(samples, loop):
    """Plain filter 0 encoder: the smallest range that fits the 16 samples of each block"""
    import math
    blocks = bytearray()
    for b in range(0, len(samples), 16):
        chunk = samples[b:b + 16]
        for rng in range(13):
            q = [round(v * 2 / (1 << rng)) for v in chunk]
            if all(-8 <= v <= 7 for v in q):
                break
        last = b + 16 >= len(samples)
        blocks.append(rng << 4 | (3 if loop else 1) * last)
        for i in range(0, 16, 2):
            blocks.append((q[i] & 15) << 4 | (q[i + 1] & 15))
    return bytes(blocks)


def snes_brr():
    """A LoROM ROM with two BRR samples side by side (a long looping one and a short one);
    returns the ROM and [(offset, loop, blocks, reference samples)]"""
    import math
    rom = bytearray(snes()[1])
    for i in range(0x1000, 0x7FA0):
        rom[i] = 0
    waves = [([int(9000 * math.sin(i / 7.0) + 4000 * math.sin(i / 3.1)) for i in range(1920)], True),
             ([int(7000 * math.sin(i / 5.0) * (1 - i / 700)) for i in range(640)], False)]
    at, found = 0x1000, []
    for samples, loop in waves:
        data = brr_encode(samples, loop)
        rom[at:at + len(data)] = data
        found.append((at, loop, len(data) // 9, brr_decode(data)))
        rom[at + len(data):at + len(data) + 0x20] = b"\xff" * 0x20   # not zeros: those would pass for silent blocks
        at += len(data) + 0x20
    return bytes(rom), found


# the palettes: three games' worth of known colors, found by their structure
MD_LEVELS = [0, 52, 87, 116, 144, 172, 206, 255]
NES_PALETTE_BYTES = bytes([0x0F, 0x30, 0x21, 0x11, 0x0F, 0x16, 0x27, 0x18, 0x0F, 0x1A, 0x29, 0x09, 0x0F, 0x02, 0x12, 0x22])


def md_palette_words(seed):
    """16 colors of the video chip (0000 BBB0 GGG0 RRR0), gradients with a black first color"""
    r = random.Random(seed)
    words = [0]
    r_, g_, b_ = r.randrange(1, 4), r.randrange(0, 4), r.randrange(0, 4)
    for i in range(15):
        r_, g_, b_ = min(7, r_ + r.randrange(0, 2)), min(7, g_ + r.randrange(0, 2)), min(7, b_ + r.randrange(0, 2))
        words.append(b_ << 9 | g_ << 5 | r_ << 1)
    return words


def md_palettes():
    """A Mega Drive ROM with 3 lines of palette at 0x3000 and the plain header; returns the ROM and the lines"""
    rom = bytearray(_md_rom())
    for i in range(0x400, len(rom)):
        rom[i] = 0
    lines = [md_palette_words(s) for s in (1, 2, 3)]
    for k, words in enumerate(lines):
        rom[0x3000 + 32 * k:0x3000 + 32 * k + 32] = b"".join(struct.pack(">H", w) for w in words)
    return bytes(rom), lines


def snes_palette_words(seed):
    """16 colors of the Super Nintendo (0BBBBBGGGGGRRRRR): a gradient with a few accents"""
    r = random.Random(seed)
    base = r.randrange(4, 14)
    words = [0]
    for i in range(1, 16):
        shade = min(31, base + i)
        words.append(((shade * 3 // 4) << 10) | (shade << 5) | (shade // 2 + r.randrange(0, 4)))
    return words


def snes_palettes():
    """A LoROM ROM with 10 lines of palette at 0x3000; returns the ROM and the lines"""
    rom = bytearray(snes()[1])
    for i in range(0x1000, 0x7FA0):
        rom[i] = 0
    lines = [snes_palette_words(s) for s in range(10)]
    for k, words in enumerate(lines):
        rom[0x3000 + 32 * k:0x3000 + 32 * k + 32] = b"".join(struct.pack("<H", w) for w in words)
    # bytes that are no colors around the block: zeros before it would pass for the black first colors of a
    # palette, and the exact start of a block cannot be told then
    rom[0x2FC0:0x3000] = b"\xff" * 0x40
    rom[0x3000 + 320:0x3000 + 320 + 0x40] = b"\xff" * 0x40
    return bytes(rom), lines


def nes_palettes():
    """A NES ROM with a palette in the program at 0x1000 (and tiles), returns the file"""
    blob = bytearray(nes()[0])
    blob[16 + 0x1000:16 + 0x1000 + 16] = NES_PALETTE_BYTES
    return bytes(blob)


def pcm_samples(length, seed):
    """Unsigned 8 bits sound: two sines and a little noise around 0x80, as a sampled drum would be"""
    r = random.Random(seed)
    return bytes(max(0, min(255, int(128 + 55 * math.sin(i / 5.0) + 25 * math.sin(i / 1.7) * (1 - i / length) + r.randrange(-3, 4)))) for i in range(length))


def md_pcm():
    """A Mega Drive ROM with a sampled sound of 3000 bytes at 0x3000 between zeros; returns the ROM and the samples"""
    rom = bytearray(_md_rom())
    for i in range(0x400, len(rom)):
        rom[i] = 0
    data = pcm_samples(3000, 5)
    rom[0x3000:0x3000 + len(data)] = data
    return bytes(rom), data


def all_fixtures():
    return [("test.nes", nes()[0]), ("test.sfc", snes()[0]), ("packed.sfc", snes_compressed()[0]), ("sized.sfc", snes_lzss()[0]), ("sizedp.sfc", snes_lzss(False)[0]), ("packedbe.sfc", snes_compressed("big")[0]), ("packedm.sfc", snes_compressed("metroid")[0]), ("brr.sfc", snes_brr()[0]),
            ("copier.smc", snes(True)[0]), ("pal.sfc", snes_palettes()[0]), ("pal.md", md_palettes()[0]), ("pcm.md", md_pcm()[0]), ("pal.nes", nes_palettes()), ("test.md", megadrive()[0]), ("packed.md", md_compressed()[0]), ("test.smd", smd()[0])]
