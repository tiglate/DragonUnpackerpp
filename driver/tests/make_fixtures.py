#!/usr/bin/env python3
"""Generate one tiny archive per format of the default driver in a directory:
python3 make_fixtures.py outdir
All of them hold the same 3 payloads (the names depend on the format)."""
import math
import struct, sys, os

HELLO = b"Hello, Dragon UnPACKer!\n"
DATA = bytes(range(256)) * 300      # > 64 KiB: several write chunks
EMPTY = b""
PAYLOADS = [HELLO, DATA, EMPTY]
LONG = ["hello.txt", "sub/dir/data.bin", "empty.dat"]
SHORT = ["HELLO", "DATA", "EMPTY"]


def z(name, n):
    return name.encode().ljust(n, b"\0")


def pak():
    data, ents, off = b"", [], 12
    for n, p in zip(LONG, PAYLOADS):
        ents.append(struct.pack("<56sII", z(n, 56), off, len(p)))
        data += p
        off += len(p)
    d = b"".join(ents)
    return b"PACK" + struct.pack("<II", off, len(d)) + data + d


def wad_dir(ident, entry):
    data, ents, off = b"", [], 12
    for i, p in enumerate(PAYLOADS):
        ents.append(entry(i, off, p))
        data += p
        off += len(p)
    return ident + struct.pack("<II", len(PAYLOADS), off) + data + b"".join(ents)


def iwad(ident=b"IWAD"):
    return wad_dir(ident, lambda i, o, p: struct.pack("<II8s", o, len(p), z(SHORT[i], 8)))


def wad2():
    # type 64 = pal, 68 = mm, 70 = pcp (see drv_fmt_wad2.pas)
    types = [64, 68, 70]
    return wad_dir(b"WAD2", lambda i, o, p: struct.pack("<IIIBBH16s", o, len(p), len(p), types[i], 0, 0, z(SHORT[i], 16)))


def gob():
    data, ents, off = b"", [], 8
    for n, p in zip(SHORT, PAYLOADS):
        ents.append(struct.pack("<II13s", off, len(p), z(n + ".TXT", 13)))
        data += p
        off += len(p)
    return b"GOB\n" + struct.pack("<I", off) + data + struct.pack("<I", len(ents)) + b"".join(ents)


def grp():
    head = b"KenSilverman" + struct.pack("<I", len(PAYLOADS))
    d = b"".join(struct.pack("<12sI", z(n + ".DAT", 12), len(p)) for n, p in zip(SHORT, PAYLOADS))
    return head + d + b"".join(PAYLOADS)


def spe():
    """Abuse .SPE: an image, a palette and a normal file (flags 0), then nothing"""
    items = [(4, b"HELLO"), (2, b"DATA"), (14, b"EMPTY")]
    head = b"SPEC1.0\0" + struct.pack("<H", len(items))
    dirsize = sum(2 + len(n) + 1 + 1 + 8 for _, n in items)
    off, ents, data = len(head) + dirsize, b"", b""
    for (t, n), p in zip(items, PAYLOADS):
        ents += struct.pack("<BB", t, len(n) + 1) + n + b"\0" + struct.pack("<BII", 0, len(p), off)
        data += p
        off += len(p)
    return head + ents + data


def dpa():
    """Drake Paint .DPA: a palette, a modern item (a PNG with editor data after it) and a link to the palette"""
    png = b"\x89PNG\r\n\x1a\n" + bytes(range(40))
    item = struct.pack("<BBI", 1, 0, len(png)) + png + b"editor"
    recs = [(2, 0, b"pal", b"", PAYLOADS[0]), (64, 0, b"hud", b"", item), (2, 1, b"pal2", b"pal", b"")]
    size = 16 + sum(1 + 1 + 4 + len(n) + 4 + len(t) + 16 for _, _, n, t, _ in recs)
    ents, data, off = b"", b"", size
    for t, lk, n, tg, p in recs:
        ents += struct.pack("<BBI", t, lk, len(n)) + n + struct.pack("<I", len(tg)) + tg
        ents += struct.pack("<QQ", 0 if lk else off, 0 if lk else len(p))
        data += p
        off += len(p)
    return b"DRAKEARC" + struct.pack("<II", 1, len(recs)) + ents + data


def rff():
    """Blood RFF 3.1: encrypted directory, HELLO.TXT flagged as encrypted"""
    names = [(b"HELLO", b"TXT", 0x10), (b"DATA", b"BIN", 0), (b"EMPTY", b"", 0)]
    data, ents, off = b"", [], 32
    for (n, e, fl), p in zip(names, PAYLOADS):
        body = bytearray(p)
        if fl & 0x10:
            for i in range(min(256, len(body))):
                body[i] ^= (i >> 1) & 0xFF
        ents.append(struct.pack("<16sIIIIB3s8sI", b"", off, len(p), 0, 0, fl, e, n, 0))
        data += bytes(body)
        off += len(p)
    d = bytearray(b"".join(ents))
    for i in range(len(d)):
        d[i] ^= ((2 * off + i) >> 1) & 0xFF
    return b"RFF\x1a" + struct.pack("<IIII", 0x301, off, len(ents), 0) + b"\0" * 12 + data + bytes(d)


def art():
    """Build tiles 10..12: 6x4 (24 bytes), 320x240 (76800 bytes), 0x0 (skipped)"""
    dims = [(6, 4), (320, 240), (0, 0)]
    head = struct.pack("<IIII", 1, 3, 10, 12)
    head += b"".join(struct.pack("<H", w) for w, h in dims) + b"".join(struct.pack("<H", h) for w, h in dims)
    head += b"\0" * 12
    return head + HELLO + DATA


def far():
    data, ents, off = b"", [], 16
    for n, p in zip(SHORT, PAYLOADS):
        ents.append((n + ".DAT", off, len(p)))
        data += p
        off += len(p)
    d = struct.pack("<I", len(ents)) + b"".join(
        struct.pack("<IIII", sz, sz, o, len(n)) + n.encode() for n, o, sz in ents)
    return b"FAR!byAZ" + struct.pack("<II", 1, off) + data + d


def vp():
    """Directories data/ and data/sub/ then back with '..'"""
    data, items, off = b"", [], 16
    def d(n):
        items.append(struct.pack("<II32sI", 0, 0, z(n, 32), 0))
    def f(n, p):
        nonlocal data, off
        items.append(struct.pack("<II32sI", off, len(p), z(n, 32), 0))
        data += p
        off += len(p)
    d("data"); f("hello.txt", HELLO); d("sub"); f("data.bin", DATA); d(".."); d("..")
    return b"VPVP" + struct.pack("<III", 2, off, len(items)) + data + b"".join(items)


def erf():
    data, ents, off = b"", [], 32 + 72 * len(PAYLOADS)
    for n, p in zip(SHORT, PAYLOADS):
        ents.append(struct.pack("<64sII", (n + ".dds").encode("utf-16-le"), off, len(p)))
        data += p
        off += len(p)
    return "ERF V2.0".encode("utf-16-le") + struct.pack("<IIII", len(ents), 0, 0, 0) + b"".join(ents) + data


def hog():
    return b"DHF" + b"".join(struct.pack("<13sI", z(n, 13), len(p)) + p
                              for n, p in zip(["HELLO.TXT", "DATA.BIN", "EMPTY.DAT"], PAYLOADS))


def bigf():
    names = ["data/hello.txt", "data/sub/data.bin", "empty.dat"]
    table_len = sum(9 + len(n) for n in names)
    off = 16 + table_len
    table, data = b"", b""
    for n, p in zip(names, PAYLOADS):
        table += struct.pack(">II", off, len(p)) + n.encode() + b"\0"
        data += p
        off += len(p)
    head = b"BIGF" + struct.pack("<I", 16 + len(table) + len(data)) + struct.pack(">II", len(names), 16 + len(table))
    return head + table + data


def drs():
    """2 tables (2 files + 1 file); the 3rd file is empty and points to a RIFF/WAVE signature"""
    data_off = 64 + 2 * 12
    blobs = [PAYLOADS[0], PAYLOADS[1], b"RIFF\0\0\0\0WAVE"]
    offs, o = [], data_off
    for b in blobs:
        offs.append(o)
        o += len(b)
    t1, t2 = o, o + 24
    head = b"Copyright (c) 1997 Ensemble Studios." + b"\x1a" * 4 + b"1.00" + z("tribe", 12)
    head += struct.pack("<II", 2, data_off)
    tables = struct.pack("<III", 0x62696e61, t1, 2) + struct.pack("<III", 0x736c7020, t2, 1)
    files = struct.pack("<III", 1, offs[0], len(PAYLOADS[0])) + struct.pack("<III", 2, offs[1], len(PAYLOADS[1]))
    files2 = struct.pack("<III", 3, offs[2], 0)
    return head + tables + b"".join(blobs) + files + files2


def wstr(t):
    return struct.pack("<I", len(t)) + t.encode("utf-16-le")


def bar_aoe3():
    names = ["hello.txt", "data.bin", "empty.dat"]
    base = "art\\"
    head = bytearray(292)
    head[0:4] = b"ESPN"
    data, off = b"", 292
    offs = []
    for p in PAYLOADS:
        offs.append(off); data += p; off += len(p)
    d = wstr(base) + struct.pack("<I", len(names))
    for n, p, o in zip(names, PAYLOADS, offs):
        d += struct.pack("<II20s", o, len(p), b"") + wstr(n)
    head[280:288] = struct.pack("<II", len(names), off)
    return bytes(head) + data + d


def bar_aom():
    names = ["hello.txt", "data.bin", "empty.dat"]
    off = 28
    data, offs = b"", []
    for p in PAYLOADS:
        offs.append(off); data += p; off += len(p)
    d = b"\0" * (4 * len(names))
    for n, p, o in zip(names, PAYLOADS, offs):
        d += struct.pack("<IIIII", o, len(p), len(p), 7, 9) + n.encode() + b"\0"
    head = b"\0" * 8 + struct.pack("<IIIII", 0, len(names), len(d), off, 8)
    return head + data + d


def hal():
    off = 32 + 32 * 3
    ents, data = b"", b""
    for n, p in zip(["hello.txt", "data.bin", "empty.dat"], PAYLOADS):
        ents += struct.pack("<IIII16s", len(p), off, 5, 6, z(n, 16))
        data += p
        off += len(p)
    return b"APUK" + struct.pack("<I", 3) + b"\0" * 24 + ents + data


def bun():
    off, data, d = 16, b"", b""
    for n, e, p in zip(["HELLO", "DATA", "EMPTY"], ["TXT", "BIN", "DAT"], PAYLOADS):
        d += z(n, 8) + z(e, 4) + struct.pack(">II", off, len(p))
        data += p
        off += len(p)
    return b"LB83" + struct.pack(">III", off, 3, 0x12345678) + data + d


def bkf():
    off = 4 + 44 * 3
    ents, data = b"", b""
    for n, p in zip(["HELLO.TXT", "sub\\DATA.BIN", "EMPTY.DAT"], PAYLOADS):
        ents += struct.pack("<36sII", z(n, 36), off, len(p))
        data += p
        off += len(p)
    return struct.pack("<I", 3) + ents + data


def gunlok():
    """2 wave entries (the 3rd one is not RIFF and is ignored); data = from the RIFF tag"""
    names = ["hello", "data", "skipped"]
    table_len = sum(4 + len(n) + 12 for n in names)
    pos = 20 + table_len
    table, chunks = b"", b""
    for n, p in zip(names, PAYLOADS):
        riff = n != "skipped"
        body = (b"RIFF" + p[4:] if riff else b"XXXX" + p[4:]) if len(p) >= 4 else p
        chunk = b"CHUNK000" + struct.pack("<I", len(body) + 48) + b"\0" * 32 + body
        table += struct.pack("<I", len(n)) + n.encode() + struct.pack("<III", 1, 2, pos + len(chunks))
        chunks += chunk
    return b"FILECHNK" + struct.pack("<III", 0, 0, len(names)) + table + chunks


def syn():
    off = 16 + 32 * 3
    ents, data = b"", b""
    for n, p in zip(["hello.txt", "data.bin", "empty.dat"], PAYLOADS):
        ents += struct.pack("<24sII", z(n, 24), off, len(p))
        data += p
        off += len(p)
    return b"FNYS" + struct.pack("<I", 3) + b"\0" * 8 + ents + data


def vol():
    names = ["hello.txt", "data.bin", "empty.dat"]
    blobs, offs, off = b"", [], 8
    for p in PAYLOADS:
        offs.append(off)
        blobs += b"VBLK" + struct.pack("<I", len(p)) + p
        off += 8 + len(p)
    nm = b"".join(n.encode() + b"\0" for n in names)
    vols = b"vols" + struct.pack("<I", len(nm)) + nm + (b"\0" if len(nm) % 2 else b"")
    voli_body = b"".join(struct.pack("<IIIIB", 0, i + 1, o, len(p), 0) for i, (o, p) in enumerate(zip(offs, PAYLOADS)))
    voli = b"voli" + struct.pack("<I", len(voli_body)) + voli_body
    return b"PVOL" + struct.pack("<I", off) + blobs + vols + voli


def pod1():
    off = 84 + 40 * 3
    ents, data = b"", b""
    for n, p in zip(["HELLO.TXT", "SUB\\DATA.BIN", "EMPTY.DAT"], PAYLOADS):
        ents += struct.pack("<32sII", z(n, 32), len(p), off)
        data += p
        off += len(p)
    return struct.pack("<I", 3) + z("Test POD", 80) + ents + data


def gta3():
    """(dir, img): files padded to 2048 bytes blocks, sizes are in blocks"""
    img, ents, block = b"", b"", 0
    for n, p in zip(["hello.txt", "data.bin", "empty.dat"], PAYLOADS):
        blocks = (len(p) + 2047) // 2048
        ents += struct.pack("<II24s", block, blocks, z(n, 24))
        img += p + b"\0" * (blocks * 2048 - len(p))
        block += blocks
    return ents, img


def dups_wad():
    """Doom maps repeat their lump names (THINGS, LINEDEFS, ..)"""
    lumps = [("MAP01", b""), ("THINGS", b"things of map 1"), ("LINEDEFS", b"lines of map 1"),
             ("MAP02", b""), ("THINGS", b"things of map 2"), ("LINEDEFS", b"lines of map 2")]
    data, ents, off = b"", [], 12
    for n, p in lumps:
        ents.append(struct.pack("<II8s", off, len(p), z(n, 8)))
        data += p
        off += len(p)
    return b"PWAD" + struct.pack("<II", len(lumps), off) + data + b"".join(ents)


def sin(count_variant=False):
    off = 12
    data, ents = b"", b""
    for n, p in zip(["hello.txt", "sub/data.bin", "empty.dat"], PAYLOADS):
        ents += struct.pack("<120sII", z(n, 120), off, len(p))
        data += p
        off += len(p)
    dirlen = 3 if count_variant else len(ents)     # v5 read it as a number of entries
    return b"SPAK" + struct.pack("<II", off, dirlen) + data + ents


def h3snd():
    off = 4 + 48 * 3
    ents, data = b"", b""
    for n, e, p in zip(["HELLO", "DATA", "EMPTY"], ["wav", "bin", "txt"], PAYLOADS):
        ents += z(n + "\0" + e, 40) + struct.pack("<II", off, len(p))
        data += p
        off += len(p)
    return struct.pack("<I", 3) + ents + data


def nascar():
    off = 2 + 27 * 3
    ents, data = b"", b""
    for n, p in zip(["HELLO.TXT", "DATA.BIN", "EMPTY.DAT"], PAYLOADS):
        ents += struct.pack("<HII13sI", 1, len(p), len(p), z(n, 13), off)
        data += p
        off += len(p)
    return struct.pack("<H", 3) + ents + data


def pff3():
    data, offs, off = b"", [], 20
    for p in PAYLOADS:
        offs.append(off)
        data += p
        off += len(p)
    ents = b"".join(struct.pack("<IIII16s", 0, o, len(p), 0, z(n, 16)) + b"\0" * 4
                    for o, p, n in zip(offs, PAYLOADS, ["hello.txt", "data.bin", "empty.dat"]))
    return struct.pack("<I4sIII", 20, b"PFF3", 3, 36, off) + data + ents      # entries of 36 bytes


def spellforce():
    names = [("data", "hello.txt"), ("data", "data.bin"), ("other", "empty.dat")]
    n = len(names)
    base_names = b""
    dir_off = {}
    for d in sorted({d for d, _ in names}):
        dir_off[d] = len(base_names)
        base_names += d[::-1].encode() + b"\0"
    name_off = []
    for _, f in names:
        name_off.append(len(base_names))
        base_names += b"\0\0" + f[::-1].encode() + b"\0"
    data, rel, fat = b"", 0, b""
    for (d, f), no, p in zip(names, name_off, PAYLOADS):
        # the upper byte of the offsets is a flag
        fat += struct.pack("<IIII", len(p), rel, no | 0x01000000, dir_off[d] | 0x02000000)
        data += p
        rel += len(p)
    data_offset = 92 + n * 16 + len(base_names)
    header = struct.pack("<I", 4) + b"MASSIVE PAKFILE V 4.0\r\n".ljust(24, b"\0") + b"\0" * 44
    header += struct.pack("<IIIII", 0, n, 0, data_offset, data_offset + len(data))
    assert len(header) == 92
    return header + fat + base_names + data


def zfs():
    """Directory in 2 blocks (2 + 1 entries): block 1 right after the header, block 2 elsewhere"""
    names = ["hello.txt", "data.bin", "empty.dat"]
    block2 = 32 + 2 * 36
    data_off = block2 + 4 + 36
    offs, o = [], data_off
    for p in PAYLOADS:
        offs.append(o); o += len(p)
    def ent(i):
        return struct.pack("<16sIIIII", z(names[i], 16), offs[i], 0, len(PAYLOADS[i]), 0, 0)
    head = b"ZFSF" + struct.pack("<IIIIIII", 1, 0, 2, 3, 0, 0, block2)
    return head + ent(0) + ent(1) + struct.pack("<I", 0) + ent(2) + b"".join(PAYLOADS)


def sni():
    names = ["HELLO", "DATA", "EMPTY"]
    data_off = 24 + 24 * 3
    total = data_off + sum(len(p) for p in PAYLOADS)
    ents, o = b"", data_off
    for n, p in zip(names, PAYLOADS):
        ents += struct.pack("<12sIII", z(n, 12), 0, o - 4, len(p))
        o += len(p)
    return struct.pack("<I", total - 4) + z("SOUNDS.SND", 12) + struct.pack("<II", 0, 3) + ents + b"".join(PAYLOADS)


def pbo():
    names = ["data\\hello.txt", "data\\sub\\data.bin", "empty.dat"]
    head = b"".join(n.encode() + b"\0" + struct.pack("<IIIII", 0, len(p), 0, 0, len(p)) for n, p in zip(names, PAYLOADS))
    head += b"\0" + b"\0" * 20
    return head + b"".join(PAYLOADS)


def bfpak():
    import zlib
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    data, d, off = b"", struct.pack("<I", 3), 16
    for n, p in zip(names, PAYLOADS):
        d += struct.pack("<I", len(n)) + n.encode() + struct.pack("<II", off, off + len(p))
        data += p
        off += len(p)
    comp = zlib.compress(d)
    return b"PAK\x01" + struct.pack("<III", off, len(d), len(comp)) + data + comp


def indy3d():
    off = 16 + 136 * 3
    ents, data = b"", b""
    for n, p in zip(["hello.txt", "sub\\data.bin", "empty.dat"], PAYLOADS):
        ents += struct.pack("<II128s", off, len(p), z(n, 128))
        data += p
        off += len(p)
    return b"GOB " + struct.pack("<III", 20, 0, 3) + ents + data


def dwfb():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    n_off = 88 + 40 * 3
    names_blob, ent_names = b"", []
    for n in names:
        ent_names.append((n_off + len(names_blob), len(n) + 1))
        names_blob += n.encode() + b"\0"
    off = n_off + len(names_blob)
    ents, data = b"", b""
    for (no, ns), p in zip(ent_names, PAYLOADS):
        ents += struct.pack("<IIIIIII", 0, no, ns, off, len(p), 0, len(p)) + b"\0" * 12
        data += p
        off += len(p)
    head = b"DWFB" + struct.pack("<I", 1) + b"\0" * 64 + struct.pack("<IIII", 3, n_off, len(names_blob), 0)
    assert len(head) == 88
    return head + ents + names_blob + data


def ppres():
    off = 2 + 22 * 3
    ents, data = b"", b""
    for n, t, p in zip(["HELLO.TXT", "DATA.BIN", "EMPTY.DAT"], [1, 2, 2], PAYLOADS):
        ents += struct.pack("<B12sIIB", len(n), z(n, 12), off, len(p), t)
        data += p
        off += len(p)
    return struct.pack("<H", 3) + ents + data


def awf():
    off = 4 + 264 * 3
    ents, data = b"", b""
    for n, p in zip(["hello.txt", "data.bin", "empty.dat"], PAYLOADS):
        ents += struct.pack("<I260s", off, z(n, 260))
        data += p
        off += len(p)
    return struct.pack("<I", 3) + ents + data


def cub():
    def x(b):
        return bytes(c ^ 0x96 for c in b)
    off = 268 + 264 * 3
    ents, data = b"", b""
    for n, p in zip(["data\\hello.txt", "data\\data.bin", "empty.dat"], PAYLOADS):
        ents += struct.pack("<256sII", z(n, 256), off, len(p))
        data += p
        off += len(p)
    head = x(b"cub\0" + b"1.0\0") + struct.pack("<I", 3 ^ 0x96969696) + x(z("test", 256))
    return head + x(ents) + x(data)


def zanzarah():
    names = ["..\\data\\hello.txt", "data\\data.bin", "empty.dat"]
    d, rel, offs = b"", 0, []
    for p in PAYLOADS:
        offs.append(rel)
        rel += len(p)
    for n, o, p in zip(names, offs, PAYLOADS):
        d += struct.pack("<I", len(n)) + n.encode() + struct.pack("<II", o, len(p))
    return b"\0\0\0\0" + struct.pack("<I", 3) + d + b"\0\0\0\0" + b"".join(PAYLOADS)


def kapl():
    names = ["data/hello.txt", "data/data.bin", "empty.dat"]
    name_block = b"".join(n.encode() + b"\0" for n in names)
    header_len = 40
    unk_off, unk_size = header_len, 4
    idx_off = unk_off + unk_size
    idx_size = 20 * 3
    names_off = idx_off + idx_size
    data_off = names_off + len(name_block)
    idx, rel = b"", 0
    for p in PAYLOADS:
        idx += struct.pack("<IIIII", rel, 0, len(p), len(p), 7)
        rel += len(p)
    head = b"KAPL" + struct.pack("<IIIIIIIII", 1, unk_off, idx_off, names_off, data_off, unk_size, idx_size, len(name_block), rel)
    assert len(head) == 40
    return head + b"\0" * 4 + idx + name_block + b"".join(PAYLOADS)


def tljpak():
    off = 24 + 20 * 3
    ents, data = b"", b""
    # entries: hello, an empty one (ignored), data
    for p in [PAYLOADS[0], b"", PAYLOADS[1]]:
        ents += struct.pack("<IIIII", off, len(p), 1, 2, 3)
        data += p
        off += len(p)
    return b"tlj_pack0001" + struct.pack("<III", 3, 0, 0) + ents + data


def sak():
    names = ["data/hello.txt", "data/data.bin", "empty.dat"]
    # entries are written in a different order than the data
    order = [2, 0, 1]
    head_len = 10 + sum(len(names[i]) + 1 + 4 for i in order)
    offs, o = [], head_len
    for p in PAYLOADS:
        offs.append(o); o += len(p)
    ents = b"".join(names[i].encode() + b"\0" + struct.pack("<I", offs[i]) for i in order)
    return b"SAK " + struct.pack("<IH", 1, 3) + ents + b"".join(PAYLOADS)


def bsa():
    import zlib
    folders = [("data", ["hello.txt", "data.bin"]), ("sub", ["empty.dat"])]
    payload_of = {"hello.txt": PAYLOADS[0], "data.bin": PAYLOADS[1], "empty.dat": PAYLOADS[2]}
    all_files = [f for _, fl in folders for f in fl]
    file_names_blob = b"".join(f.encode() + b"\0" for f in all_files)
    folder_names_total = sum(len(d) + 1 for d, _ in folders)
    blocks_size = sum(1 + len(d) + 1 + 16 * len(fl) for d, fl in folders)
    folder_rec_off = 36
    first_block = folder_rec_off + 16 * len(folders)
    names_off = first_block + blocks_size
    data_off = names_off + len(file_names_blob)
    # data of every file (data.bin is stored compressed: original size + zlib)
    datas, offs, o = {}, {}, data_off
    for f in all_files:
        p = payload_of[f]
        d = struct.pack("<I", len(p)) + zlib.compress(p) if f == "data.bin" else p
        datas[f] = d
        offs[f] = o
        o += len(d)
    folder_recs, blocks, bpos = b"", b"", first_block
    for d, fl in folders:
        folder_recs += struct.pack("<QII", 0, len(fl), bpos + len(file_names_blob))
        block = bytes([len(d) + 1]) + d.encode() + b"\0"
        for f in fl:
            size = len(datas[f]) | (0x40000000 if f == "data.bin" else 0)
            block += struct.pack("<QII", 0, size, offs[f])
        blocks += block
        bpos += len(block)
    head = b"BSA\0" + struct.pack("<IIIIIIII", 103, 36, 3, len(folders), len(all_files), folder_names_total, len(file_names_blob), 0)
    assert len(head) == 36
    return head + folder_recs + blocks + file_names_blob + b"".join(datas[f] for f in all_files)


def slf():
    names = ["hello.txt", "data.bin", "empty.dat"]
    data, offs, off = b"", [], 532
    for p in PAYLOADS:
        offs.append(off); data += p; off += len(p)
    head = z("LIBRARY", 256) + z("data\\", 256) + struct.pack("<II", 3, 3) + bytes([0xFF, 0xFF, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0])
    assert len(head) == 532
    ents = b"".join(struct.pack("<256sII", z(n, 256), o, len(p)) + b"\0" * 16 for n, o, p in zip(names, offs, PAYLOADS))
    return head + data + ents


def rffl():
    ents = b""
    off = 20
    names = ["hello.txt", "data.bin", "empty.dat"]
    # first compute the size of the directory
    dir_size = sum(8 + ((len(n) + 1 + 3) // 4) * 4 for n in names)
    off = 20 + dir_size
    data = b""
    for n, p in zip(names, PAYLOADS):
        nb = n.encode() + b"\0"
        nb += b"\0" * ((4 - len(nb) % 4) % 4)
        ents += struct.pack("<II", off, len(p)) + nb
        data += p
        off += len(p)
    return b"RFFL" + struct.pack("<IIII", 1, 3, dir_size, len(data)) + ents + data


def prm():
    off = 16
    offs, data = [], b""
    for p in PAYLOADS:
        offs.append(off); data += p; off += len(p)
    table = b"".join(struct.pack("<I", o) for o in offs)
    return struct.pack("<IIII", 0x71E, off, 16, 3) + data + table


def cbin():
    off = 4 + 24 * 3
    ents, data = b"", b""
    for n, p in zip(["HELLO.TXT", "DATA.BIN", "EMPTY.DAT"], PAYLOADS):
        ents += struct.pack("<16sII", z(n, 16), off, len(p) + 4)
        data += struct.pack("<I", 7) + p
        off += len(p) + 4
    return struct.pack("<I", 3) + ents + data


# Payload of the RNC fixtures (see fixtures/README.txt: do not change it)
RNC_PAYLOAD = b"".join(b"Line %d: the quick brown fox jumps over the lazy dog %d\n" % (i, i * i % 97) for i in range(300))
def cpr():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    nxt_rel = 0
    ents_len = sum(12 + len(n) + 1 for n in names)
    base = 32 + 16 + ents_len
    off, ents, data = base, b"", b""
    for n, p in zip(names, PAYLOADS):
        ents += struct.pack("<III", off, len(p), 7) + n.encode() + b"\0"
        data += p
        off += len(p)
    # block points to itself: next = smallest offset + rel, with rel chosen so it equals the block start
    hdr = struct.pack("<iiii", 1, 0, 3, 32 - base)
    return z("ASCARON_ARCHIVE V0.9", 32) + hdr + ents + data


def fpk():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    n = 3
    dirlen = sum(4 + len(x) + 16 for x in names)
    off, d, data = 13 + dirlen, b"", b""
    for nm, p in zip(names, PAYLOADS):
        d += struct.pack("<I", len(nm)) + bytes(ord(c) + 1 for c in nm) + struct.pack("<iiii", 0, 0, len(p), off)
        data += p
        off += len(p)
    return struct.pack("<i", 4) + b"FPK_" + b"\x01" + struct.pack("<i", n) + d + data


def rez():
    ident = "\r\nRezMgr Version 1 Copyright (C) 1995 MONOLITH INC.           \r\nLithTech Resource File                                      \r\n\x1a"
    assert len(ident) == 127
    data = b"".join(PAYLOADS[:2])
    o0 = 163
    o1 = o0 + len(PAYLOADS[0])
    def fil(off, size, name, ext):
        return struct.pack("<iIII", 0, off, size, 0) + struct.pack("<I", 1) + ext[::-1].encode().ljust(4, b"\0") + struct.pack("<I", 0) + name.encode() + b"\0"
    sub = fil(o0, len(PAYLOADS[0]), "hello", "txt") + fil(o1, len(PAYLOADS[1]), "data", "bin")
    sub_off = 163 + len(data)
    root_off = sub_off + len(sub)
    root = struct.pack("<iIII", 1, sub_off, len(sub), 0) + b"data\0" + fil(o0, len(PAYLOADS[0]), "again", "txt")
    head = ident.encode("latin1") + struct.pack("<9i", 1, root_off, len(root), 0, 0, 0, 0, 4, 9)
    return head + data + sub + root


def lembox():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    head = b"LEMBOX" + struct.pack("<ii", 3, 0)
    nm = b"".join(struct.pack("<I", len(n) + 1) + n.encode() + b"\0" for n in names)
    off = len(head) + len(nm) + 12
    offs = b""
    for p in PAYLOADS:
        offs += struct.pack("<I", off)
        off += len(p)
    return head + nm + offs + b"".join(PAYLOADS)


def cob():
    names = ["hello.txt", "data.bin", "empty.dat"]
    off = 4 + 54 * 3
    offs = b""
    for p in PAYLOADS:
        offs += struct.pack("<I", off)
        off += len(p)
    return struct.pack("<I", 3) + b"".join(z(n, 50) for n in names) + offs + b"".join(PAYLOADS)


def eires():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    nt, noffs = b"", []
    for n in names:
        noffs.append(len(nt))
        nt += n.encode() + b"\0"
    data = b"".join(PAYLOADS)
    off = 16
    d = b""
    for n, no, p in zip(names, noffs, PAYLOADS):
        d += struct.pack("<IIIIHI", 1, len(p), off, 2, len(n) + 1, no)
        off += len(p)
    return struct.pack("<IIII", 0x019CE23C, 3, 16 + len(data), len(nt)) + data + d + nt


def bnt2():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    data = b"".join(PAYLOADS)
    off = 0
    d = struct.pack("<I", 3)
    for n, p in zip(names, PAYLOADS):
        d += n.encode() + b"\n" + struct.pack("<IIII", len(p), off, 0, 0)
        off += len(p)
    return data + d + struct.pack("<I", len(data)) + b"BNT2"


def pkr():
    # two folders: "a/" holds 2 files, "b/" holds 1
    files = [("a/", "hello.txt"), ("a/", "data.bin"), ("b/", "empty.dat")]
    base = 16 + 40 * 2 + 48 * 3
    d = struct.pack("<4siii", b"PKR2", 1, 2, 3)
    d += z("a/", 32) + struct.pack("<ii", 0, 2) + z("b/", 32) + struct.pack("<ii", 0, 1)
    off = base
    for (_, f), p in zip(files, PAYLOADS):
        d += z(f, 32) + struct.pack("<iIII", 0, off, len(p), len(p))
        off += len(p)
    return d + b"".join(PAYLOADS)


def pod2():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    nt, noffs = b"", []
    for n in names:
        noffs.append(len(nt))
        nt += n.encode() + b"\0"
    base = 0x60 + 20 * 3 + len(nt)
    d = b"POD2" + struct.pack("<i", 0) + z("test", 80) + struct.pack("<ii", 3, 0)
    off = base
    for no, p in zip(noffs, PAYLOADS):
        d += struct.pack("<IIIII", no, len(p), off, 0, 0)
        off += len(p)
    return d + nt + b"".join(PAYLOADS)


def sqh():
    sub_names = ["a.txt", "b.bin"]
    sub_len = 2 * 16
    dir_len = 4 + 12 + sub_len + 16
    off = dir_len
    sub = b""
    for n, p in zip(sub_names, PAYLOADS):
        sub += z(n, 8) + struct.pack(">II", off, len(p))
        off += len(p)
    root = z("data", 8) + struct.pack(">i", sub_len) + sub
    root += z("c.dat", 8) + struct.pack(">II", off, 0)
    return struct.pack("<I", dir_len) + root + b"".join(PAYLOADS)


def fableb():
    names = ["hello.txt", "data.bin", "empty.dat"]
    data = b"".join(PAYLOADS)
    base_off = 12
    bank_off = base_off + len(data)
    bank = struct.pack("<I", 0)
    off = base_off
    for n, p in zip(names, PAYLOADS):
        bank += struct.pack("<IIIIII", 0x2A, 1, 0, len(p), off, 0)
        bank += struct.pack("<I", len(n)) + n.encode() + struct.pack("<II", 0, 1) + struct.pack("<I", 2) + b"zz" + struct.pack("<I", 0)
        off += len(p)
    footer_off = bank_off + len(bank)
    footer = struct.pack("<I", 1) + b"banks\0" + struct.pack("<IIIII", 0, 3, bank_off, 0, 0)
    return b"BIGB" + struct.pack("<II", 0x64, footer_off) + data + bank + footer


def ltar():
    names = ["hello.txt", "data.bin", "empty.dat"]
    nt, pos = b"", {}
    for n in ["", "sub"] + names:
        pos[n] = len(nt)
        nt += n.encode() + b"\0"
    base = 48 + len(nt) + 32 * 3 + 16 * 3
    off, ents = base, b""
    for n, p in zip(names, PAYLOADS):
        ents += struct.pack("<IqqqI", pos[n], off, len(p), len(p), 0)
        off += len(p)
    # folder list: root (0 entries, skipped), "sub" with all the files
    dirs = struct.pack("<IIII", pos[""], 0, 0, 0) + struct.pack("<IIII", pos["sub"], 0, 0, 3) + struct.pack("<IIII", pos[""], 0, 0, 0)
    head = b"LTAR" + struct.pack("<iiii", 3, len(nt), 3, 3) + bytes(28)
    return head + nt + ents + dirs + b"".join(PAYLOADS)


def bag():
    names = ["hello", "data", "empty"]
    mt = [37, 2, 99]
    off = 16 + 64 * 3
    d = b"GABA" + struct.pack("<iii", 0, 3, 64)
    for n, m, p in zip(names, mt, PAYLOADS):
        d += z(n, 32) + struct.pack("<IIII", off, len(p), 22050, m) + bytes(16)
        off += len(p)
    return d + b"".join(PAYLOADS)


def opk():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    recs = []
    for n in names:
        nm = n.encode() + b"\0"
        sz = 24 + len(nm)
        sz += (-sz) % 4
        recs.append((nm, sz))
    dirsize = sum(r[1] for r in recs)
    off = 16 + dirsize
    d = b"PAK " + struct.pack("<iii", 3, dirsize, 0)
    for (nm, sz), p in zip(recs, PAYLOADS):
        d += struct.pack("<IIIIQ", off, len(p), 32, sz, 0) + nm + bytes(sz - 24 - len(nm))
        off += len(p)
    return d + b"".join(PAYLOADS)


def oaf():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    off = 24 + 20 * 3
    ents = b""
    for p in PAYLOADS:
        ents += struct.pack("<IQQ", 0, off, len(p))
        off += len(p)
    data = b"".join(PAYLOADS)
    nt = b"".join(n.encode() + b"\0" for n in names)
    return b"OAF!" + struct.pack("<IIQI", 0, 0, 24 + 60 + len(data), 3) + ents + data + nt


def europa():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    data = b"".join(PAYLOADS)
    off = 33
    blk = b""
    for n, p in zip(names, PAYLOADS):
        blk += n.encode() + b"\0" + struct.pack("<III", off, len(p), 0)
        off += len(p)
    head = b"Europa Packfile" + b"\0" + struct.pack("<i", 0) + b"\0" + struct.pack("<iii", 33 + len(data), len(blk), 3)
    assert len(head) == 33
    return head + data + blk


def tmpak():
    import zlib
    z1 = zlib.compress(PAYLOADS[0])
    blobs = [
        struct.pack("<iiii", 16 + len(z1), len(PAYLOADS[0]), len(z1), 0) + z1,
        struct.pack("<iiii", 16 + len(PAYLOADS[1]), len(PAYLOADS[1]), len(PAYLOADS[1]), 1) + PAYLOADS[1],
        struct.pack("<iiii", 16, 0, 0, 1),
    ]
    dirs = b"data\\\0" + b"\0"
    dirpos = [0, 0, 6 << 1]
    names = ["hello.txt", "data.bin", "empty.dat"]
    base = 52 + 56 * 3
    off, ents, data = base, b"", b""
    for b, dp, n, p in zip(blobs, dirpos, names, PAYLOADS):
        ents += struct.pack("<iiiiii", 0, 0, off, len(b), len(p), dp) + z(n, 32)
        data += b
        off += len(b)
    dir_off = base + len(data)
    head = struct.pack("<13i", 5, base, 0, 3, 1, 2, len(dirs), 0, 0, 0, 0x34, dir_off, dir_off)
    return head + ents + data + dirs


def ssa():
    import zlib
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    z1 = zlib.compress(PAYLOADS[0])
    blobs = [b"PK01" + struct.pack("<iiH", len(PAYLOADS[0]), 0, 0) + z1, PAYLOADS[1], b""]
    dirlen = sum(4 + len(n) + 12 for n in names)
    off = 16 + dirlen
    d = b"rass" + struct.pack("<iii", 1, 0, dirlen)
    for n, b in zip(names, blobs):
        d += struct.pack("<I", len(n)) + n.encode() + struct.pack("<iii", off, off + len(b), len(b))
        off += len(b)
    return d + b"".join(blobs)


def dzip():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    data = b"".join(PAYLOADS)
    off = 24
    ents = b""
    for n, p in zip(names, PAYLOADS):
        ents += struct.pack("<H", len(n)) + n.encode() + struct.pack("<qqqq", 0, len(p), off, len(p))
        off += len(p)
    return b"DZIP" + struct.pack("<iiiq", 2, 3, 0, 24 + len(data)) + data + ents


def pod3(v5=False):
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    nt, noffs = b"", []
    for n in names:
        noffs.append(len(nt))
        nt += n.encode() + b"\0"
    es = 28 if v5 else 20
    data = b"".join(PAYLOADS)
    off = 288
    ents = b""
    for no, p in zip(noffs, PAYLOADS):
        ents += struct.pack("<IIIII", no, len(p), off, 0, 0) + (bytes(8) if v5 else b"")
        off += len(p)
    head = (b"POD5" if v5 else b"POD3") + struct.pack("<i", 0) + z("test", 80) + struct.pack("<iiii", 3, 0, 0, 0) + bytes(160)
    head += struct.pack("<iiiiii", 288 + len(data), 0, len(nt), 0, 0, 0)
    assert len(head) == 288, len(head)
    return head + data + ents + nt


def lzss_gzp(data):
    """LZSS as used by Giants GZP; only matches at distance 256 (enough to test the decoder)."""
    items, j = [], 0
    while j < len(data):
        n = min(18, len(data) - j)
        if j >= 256 and n >= 3 and data[j:j + n] == data[j - 256:j - 256 + n]:
            v = ((j - 256) + 0xFEE) & 0xFFF
            items.append((0, bytes([v & 0xFF, ((v >> 4) & 0xF0) | (n - 3)])))
            j += n
        else:
            items.append((1, data[j:j + 1]))
            j += 1
    out = b""
    for k in range(0, len(items), 8):
        grp = items[k:k + 8]
        out += bytes([sum(f << b for b, (f, _) in enumerate(grp))]) + b"".join(x for _, x in grp)
    return out


def gzp():
    names = ["hello.txt", "data.bin", "empty.dat"]
    comp = lzss_gzp(PAYLOADS[1])
    blobs = [(PAYLOADS[0], 2, len(PAYLOADS[0])), (comp, 1, len(PAYLOADS[1])), (b"", 2, 0)]
    off = 24
    data, ents = b"", b""
    # data blocks: 16 bytes of block header then the data
    starts = []
    for b, _, _ in blobs:
        starts.append(off)
        data += bytes(16) + b
        off += 16 + len(b)
    for n, (b, flag, unc), st in zip(names, blobs, starts):
        ents += struct.pack("<iiiiBB", len(b), unc, 0, st, flag, len(n)) + n.encode()
    return struct.pack("<Iiiiii", 0x6608F101, 24 + len(data), 0, 0, 0, 0) + data + struct.pack("<ii", 0, 3) + ents


def rcf():
    import zlib
    names = ["hello.txt.rz", "data.bin", "empty.dat"]
    z1 = zlib.compress(PAYLOADS[0])
    blobs = [b"RZ" + bytes(6) + struct.pack("<q", len(PAYLOADS[0])) + z1, PAYLOADS[1], b""]
    off = 60 + 12 * 3
    offs, data = [], b""
    for b in blobs:
        offs.append(off)
        data += b
        off += len(b)
    name_off = off
    nt = struct.pack("<I", 0)
    for n in names:
        nt += bytes(16) + struct.pack("<I", len(n) + 1) + n.encode()
    blocks = b"".join(struct.pack("<III", 0, o, len(b)) for o, b in reversed(list(zip(offs, blobs))))
    head = z("ATG CORE CEMENT LIBRARY", 32) + struct.pack("<IIIIIII", 0, 0, 0, name_off, 0, 0, 3)
    return head + blocks + data + nt


def lz_mtf(data):
    """Darkstone LZ; only matches at distance 256 (max length 66)."""
    items, j = [], 0
    while j < len(data):
        n = min(66, len(data) - j)
        if j >= 256 and n >= 3 and data[j:j + n] == data[j - 256:j - 256 + n]:
            items.append((0, struct.pack("<H", (n - 3) * 1024 + 256)))
            j += n
        else:
            items.append((1, data[j:j + 1]))
            j += 1
    out = b""
    for k in range(0, len(items), 8):
        grp = items[k:k + 8]
        out += bytes([sum(f << b for b, (f, _) in enumerate(grp))]) + b"".join(x for _, x in grp)
    return out


def mtf():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    comp = bytes([174, 190, 0, 0]) + struct.pack("<ii", 1, 0) + lz_mtf(PAYLOADS[1])
    blobs = [PAYLOADS[0], comp, b""]
    sizes = [len(p) for p in PAYLOADS]
    dirlen = 4 + sum(4 + len(n) + 8 for n in names)
    off, d, data = dirlen, struct.pack("<I", 3), b""
    for n, b, sz in zip(names, blobs, sizes):
        d += struct.pack("<I", len(n)) + n.encode() + struct.pack("<II", off, sz)
        data += b
        off += len(b)
    return d + data


def lzo_rfa(data):
    """Minimal LZO1X stream: a literal run, then M3 matches at distance 256, then the end marker."""
    def lit_run(chunk):
        n = len(chunk) - 3
        if n <= 15:
            return bytes([n]) + chunk
        n -= 15
        return b"\0" + b"\0" * (n // 255) + bytes([n % 255]) + chunk
    out = lit_run(data[:256])
    j = 256
    while j < len(data):
        n = min(33, len(data) - j)
        if n == 2:           # M3 needs a length of at least 3 in this encoder
            raise ValueError
        out += bytes([32 | (n - 2)]) + struct.pack("<H", (256 - 1) << 2)
        j += n
    return out + bytes([17, 0, 0])


def rfa(beta=False):
    names = ["data/hello.txt", "data/data.bin", "empty.dat"]
    comp = lzo_rfa(PAYLOADS[1])
    seg = struct.pack("<I", 1) + struct.pack("<III", len(comp), len(PAYLOADS[1]), 0) + comp
    blobs = [PAYLOADS[0], seg, b""]
    start = 32 if beta else 8
    off, ents, data = start, b"", b""
    for n, p, b in zip(names, PAYLOADS, blobs):
        if beta:
            ents += struct.pack("<I", len(n)) + n.encode() + struct.pack("<IIIIII", len(p), len(p), off, 0, 0, 0)
            data += p
            off += len(p)
        else:
            ents += struct.pack("<I", len(n)) + n.encode() + struct.pack("<IIIIII", len(b), len(p), off, 0, 0, 0)
            data += b
            off += len(b)
    dirt = struct.pack("<I", 3) + ents
    if beta:
        return b"Refractor2 FlatArchive 1.1  " + struct.pack("<I", off) + data + dirt
    return struct.pack("<II", off, 1) + data + dirt


def vfs():
    import zlib
    BS, WIN = 160, 50000
    def ent(name, typ, rel, size, orig):
        return z(name, 64) + struct.pack("<iiiiii", 0, typ, -1, rel, size, orig)
    def pad(b, n=BS):
        return b + bytes((-len(b)) % n)
    c1, c2 = zlib.compress(PAYLOADS[1][:WIN]), zlib.compress(PAYLOADS[1][WIN:])
    chunks = struct.pack("<I", len(c1)) + c1 + struct.pack("<III", 8, 1, 2) + struct.pack("<I", len(c2)) + c2
    root = ent("data", 2, 1, 2 * 88, 0) + ent("empty.dat", 1, 1, 0, 0)
    sub = ent("hello.txt", 1, 3, len(PAYLOADS[0]), 0) + ent("data.bin", 9, 4, len(chunks), len(PAYLOADS[1]))
    # blocks: 1-2 = sub directory (176 bytes = 2 blocks), 3 = hello, 4.. = chunks
    body = pad(sub) + pad(PAYLOADS[0]) + chunks
    head = struct.pack("<fIiiiiI", 1.0, BS, 0, 2, 0, 64, WIN) + bytes(16) + struct.pack("<i", 0)
    assert len(head) == 48
    return head + struct.pack("<i", 0) + root + body


def lz77_hpi(data):
    """Total Annihilation LZ77 (4096 bytes window, first position 1); matches at distance 256 only."""
    items, j = [], 0
    while j < len(data):
        n = min(17, len(data) - j)
        pos = (j - 256 + 1) & 0xFFF
        ok = j >= 256 and n >= 2 and all(((pos + k) & 0xFFF) != 0 for k in range(n)) and data[j:j + n] == data[j - 256:j - 256 + n]
        if ok:
            items.append((1, struct.pack("<H", (pos << 4) | (n - 2))))
            j += n
        else:
            items.append((0, data[j:j + 1]))
            j += 1
    items.append((1, b"\0\0"))
    out = b""
    for k in range(0, len(items), 8):
        grp = items[k:k + 8]
        out += bytes([sum(f << b for b, (f, _) in enumerate(grp))]) + b"".join(x for _, x in grp)
    return out


def hpi(key_header=0x7D):
    import zlib
    HK = key_header
    key = (~(((HK * 4) | (HK >> 6)) & 0xFFFFFFFF)) & 0xFF if HK else 0
    # --- directory (offsets are absolute: the directory starts at 20)
    names = ["data", "hello.txt", "data.bin", "empty.dat"]
    nbytes = b"".join(n.encode() + b"\0" for n in names)
    root_tbl = 20
    root_ents = root_tbl + 8
    sub_tbl = root_ents + 18
    sub_ents = sub_tbl + 8
    recs = sub_ents + 18          # file records (9 bytes each): hello, data.bin, empty
    names_at = recs + 27
    nofs, p = {}, names_at
    for n in names:
        nofs[n] = p
        p += len(n) + 1
    dir_end = p
    # --- data
    chunks = [DATA_CHUNK for DATA_CHUNK in (PAYLOADS[1][:65536], PAYLOADS[1][65536:])]
    def scramble(b):
        return bytes(((c ^ i) + i) & 0xFF for i, c in enumerate(b))
    z1 = scramble(zlib.compress(chunks[0]))
    l2 = lz77_hpi(chunks[1])
    c1 = b"SQSH" + bytes([2, 2, 1]) + struct.pack("<III", len(z1), len(chunks[0]), 0) + z1
    c2 = b"SQSH" + bytes([2, 1, 0]) + struct.pack("<III", len(l2), len(chunks[1]), 0) + l2
    hello_at = dir_end
    bin_at = hello_at + len(PAYLOADS[0])
    binblob = struct.pack("<II", len(c1), len(c2)) + c1 + c2
    end = bin_at + len(binblob)
    d = struct.pack("<II", 2, root_ents)
    d += struct.pack("<IIB", nofs["data"], sub_tbl, 1) + struct.pack("<IIB", nofs["empty.dat"], recs + 18, 0)
    d += struct.pack("<II", 2, sub_ents)
    d += struct.pack("<IIB", nofs["hello.txt"], recs, 0) + struct.pack("<IIB", nofs["data.bin"], recs + 9, 0)
    d += struct.pack("<IIB", hello_at, len(PAYLOADS[0]), 0) + struct.pack("<IIB", bin_at, len(PAYLOADS[1]), 2) + struct.pack("<IIB", end, 0, 0)
    d += nbytes
    assert 20 + len(d) == dir_end, (20 + len(d), dir_end)
    body = d + PAYLOADS[0] + binblob
    head = b"HAPI" + b"\x00\x00\x01\x00" + struct.pack("<III", dir_end, HK, 20)
    plain = head + body
    enc = plain[:20] + bytes((~(c ^ ((i + 20) ^ key))) & 0xFF for i, c in enumerate(plain[20:])) if key else plain
    return enc


def pkpak():
    import zlib
    names = ["data/hello.txt", "data/data.bin", "unused", "again.txt"]
    z1 = zlib.compress(PAYLOADS[1])
    data, pos = b"", 5
    meta = []
    for blob, plain in ((PAYLOADS[0], PAYLOADS[0]), (z1, PAYLOADS[1])):
        meta.append((pos, len(plain), len(blob)))
        data += blob
        pos += len(blob)
    meta.append((0, 0, 0))
    meta.append(meta[0])
    idx = struct.pack("<I", len(names))
    for i, (n, (o, sz, em)) in enumerate(zip(names, meta), 1):
        L = len(n)
        key = (L * 2 + 1 + [-2, -1, 0, 1, 2][L % 5] + i) & 0xFF
        enc = bytes((ord(c) ^ ((key + 2 * k) & 0xFF)) for k, c in enumerate(n))
        idx += struct.pack("<I", L) + enc + struct.pack("<III", o, sz, em)
    return b"\x00" + struct.pack("<I", pos) + data + idx


def dune_rfh():
    import zlib
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    z1 = zlib.compress(PAYLOADS[1])
    pk = struct.pack("<I", len(PAYLOADS[1])) + z1
    rfd = PAYLOADS[0] + pk
    recs = [(0, len(PAYLOADS[0]), len(PAYLOADS[0]), 0), (2, len(pk), len(PAYLOADS[1]), len(PAYLOADS[0])), (0, 0, 0, len(rfd))]
    rfh = b""
    for n, (fl, pks, orig, off) in zip(names, recs):
        rfh += struct.pack("<IIIIII", len(n), 0, fl, pks, orig, off) + n.encode() + b"\0"
    return rfh, rfd


def ppxrs():
    names = ["HELLO.TXT", "DATA.BIN", "EMPTY.DAT"]
    off, d, data = 2 + 32 * 3, struct.pack("<H", 3), b""
    for n, p in zip(names, PAYLOADS):
        d += bytes([len(n)]) + z(n, 12) + struct.pack("<II", off, len(p)) + bytes(11)
        data += p
        off += len(p)
    return d + data


def eve_stuff():
    names = ["res:/hello.txt", "res:/data.bin", "empty.dat"]
    d = struct.pack("<I", 3)
    for n, p in zip(names, PAYLOADS):
        d += struct.pack("<II", len(p), 0) + n.encode() + b"\0"
    return d + b"".join(PAYLOADS)


def tgp(version):
    import gzip, io
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    n = 3
    base = 16 + 112 * n
    if version == 1:
        off, ents, data = base, b"", b""
        for nm, p in zip(names, PAYLOADS):
            ents += z(nm, 96) + struct.pack("<IIII", off, 0, len(p), 0)
            data += bytes(c ^ 0x46 for c in p)
            off += len(p)
        return b"TGP0" + struct.pack("<HHII", 1, 0, 1, n) + ents + data
    off, ents, block = 0, b"", b""
    for nm, p in zip(names, PAYLOADS):
        ents += z(nm, 96) + struct.pack("<IIII", off, len(p), 0, 0)
        block += p
        off += len(p)
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", mtime=0, filename="x") as g:
        g.write(block)
    return b"TGP0" + struct.pack("<HHII", 2, 2, n, len(block)) + ents + buf.getvalue()


def tonga():
    import lzma
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    alone = lzma.compress(PAYLOADS[1], format=lzma.FORMAT_ALONE, filters=[{"id": lzma.FILTER_LZMA1, "preset": 6, "lc": 3, "lp": 0, "pb": 2}])
    alone = alone[:5] + struct.pack("<Q", len(PAYLOADS[1])) + alone[13:]      # the size is -1 in the header written by liblzma
    blobs = [(PAYLOADS[0], 0), (alone, 1), (b"", 0)]
    dirlen = sum(12 + 4 + len(n) for n in names)
    off = 26 + dirlen
    d = b""
    data = b""
    for n, (b, fl) in zip(names, blobs):
        d += struct.pack("<III", off, len(b), fl) + struct.pack("<I", len(n)) + n.encode()
        data += b
        off += len(b)
    return b"tongas_pack_v30000" + struct.pack("<II", 3, dirlen) + d + data


def godot_pck(version):
    names = ["res://data/hello.txt", "res://data/data.bin", "res://empty.dat"]
    n = len(names)
    def ent(nm, off, p):
        nb = nm.encode() + b"\0"
        nb += bytes((-len(nb)) % 4)
        e = struct.pack("<I", len(nb)) + nb + struct.pack("<QQ", off, len(p)) + bytes(16)
        return e + (struct.pack("<I", 0) if version >= 2 else b"")
    esize = sum(len(ent(nm, 0, p)) for nm, p in zip(names, PAYLOADS))
    if version <= 1:
        head = b"GDPC" + struct.pack("<IIII", version, 3, 4, 4) + bytes(64) + struct.pack("<I", n)
        base = len(head) + esize
    elif version == 2:
        head = b"GDPC" + struct.pack("<IIII", 2, 4, 0, 0) + struct.pack("<IQ", 0, 0) + bytes(64) + struct.pack("<I", n)
        base = len(head) + esize
    else:
        # format 3: the directory is after the files
        head = b"GDPC" + struct.pack("<IIII", 3, 4, 4, 0) + struct.pack("<IQQ", 0, 0, 0) + bytes(64)
        base = len(head)
    off, ents, data = (0 if version >= 2 else base), b"", b""
    if version == 3:
        off = 0
    for nm, p in zip(names, PAYLOADS):
        ents += ent(nm, off, p)
        data += p
        off += len(p)
    if version == 3:
        dir_off = len(head) + len(data)
        head = head[:24] + struct.pack("<QQ", base, dir_off) + head[40:]
        return head + data + struct.pack("<I", n) + ents
    if version == 2:
        head = head[:20] + struct.pack("<IQ", 0, base) + head[32:]
    return head + ents + data


def chromium_pak(version):
    ids = [101, 102, 103]
    blobs = PAYLOADS
    if version == 5:
        head = struct.pack("<I", 5) + bytes([1, 0, 0, 0]) + struct.pack("<HH", 3, 1)
    else:
        head = struct.pack("<II", 4, 3) + bytes([1])
    off = len(head) + 6 * 4 + (4 if version == 5 else 0)
    table = b""
    for i, b in zip(ids, blobs):
        table += struct.pack("<HI", i, off)
        off += len(b)
    table += struct.pack("<HI", 0, off)
    alias = struct.pack("<HH", 200, 0) if version == 5 else b""
    return head + table + alias + b"".join(blobs)


def unreal_pak(version, names_count=5, encoded=None):
    import zlib
    if encoded is None:
        encoded = version >= 10
    files = [("data/hello.txt", PAYLOADS[0], 0), ("data/data.bin", PAYLOADS[1], 1), ("empty.dat", PAYLOADS[2], 0)]
    comp_names = ["Zlib", "Oodle"]
    BS = 65536
    method_zlib = 1 if version >= 8 else 1
    def fstr(t):
        return struct.pack("<i", len(t) + 1) + t.encode() + b"\0"
    data, recs = b"", []
    for nm, p, comp in files:
        off = len(data)
        if comp:
            chunks = [p[i:i + BS] for i in range(0, len(p), BS)] or [b""]
            zs = [zlib.compress(c) for c in chunks]
            hdr = 48 + 4 + 16 * len(zs) + 5
            blocks, pos = [], hdr
            for z_ in zs:
                blocks.append((pos, pos + len(z_)))
                pos += len(z_)
            rel = blocks if version >= 5 else [(a + off, b + off) for a, b in blocks]
            size = sum(len(z_) for z_ in zs)
            rec = dict(off=off, size=size, usize=len(p), method=method_zlib, blocks=rel, bs=BS, body=b"".join(zs), n=len(zs))
        else:
            rec = dict(off=off, size=len(p), usize=len(p), method=0, blocks=[], bs=0, body=p, n=0)
        recs.append(rec)
        cp = dict(rec, off=0)
        data += ser_entry(version, cp, bytes(20)) + rec["body"]
    # index
    mount = fstr("../../../Game/")
    if not encoded:
        idx = mount + struct.pack("<I", len(files))
        for (nm, _, _), rec in zip(files, recs):
            idx += fstr(nm) + ser_entry(version, rec, bytes(20))
    else:
        enc = b""
        locs = []
        for rec in recs:
            locs.append(len(enc))
            v = rec["n"] << 6
            bs = rec["bs"]
            extra = b""
            if bs % 2048 == 0 and (bs >> 11) < 0x3F:
                v |= bs >> 11
            else:
                v |= 0x3F
                extra = struct.pack("<I", bs)
            v |= rec["method"] << 23
            fields = b""
            if rec["off"] < 2 ** 32:
                v |= 1 << 31; fields += struct.pack("<I", rec["off"])
            else:
                fields += struct.pack("<Q", rec["off"])
            if rec["usize"] < 2 ** 32:
                v |= 1 << 30; fields += struct.pack("<I", rec["usize"])
            else:
                fields += struct.pack("<Q", rec["usize"])
            if rec["method"]:
                if rec["size"] < 2 ** 32:
                    v |= 1 << 29; fields += struct.pack("<I", rec["size"])
                else:
                    fields += struct.pack("<Q", rec["size"])
            sizes = b""
            if rec["n"] > 1:
                sizes = b"".join(struct.pack("<I", b - a) for a, b in rec["blocks"])
            enc += struct.pack("<I", v) + extra + fields + sizes
        dirs = [("Game/data/", [("hello.txt", locs[0]), ("data.bin", locs[1])]), ("Game/", [("empty.dat", locs[2])])]
        dir_idx = struct.pack("<I", len(dirs))
        for dn, fl in dirs:
            dir_idx += fstr(dn) + struct.pack("<I", len(fl)) + b"".join(fstr(n) + struct.pack("<i", l) for n, l in fl)
        idx = fstr("../../../") + struct.pack("<I", len(files)) + struct.pack("<Q", 0x1234)
        full_off = len(data)       # filled below
    base_index_off = len(data)
    if encoded:
        # the directory index goes first, then the primary index
        full_off = base_index_off
        primary_off = full_off + len(dir_idx)
        idx += struct.pack("<I", 0)                          # no path hash index
        idx += struct.pack("<I", 1) + struct.pack("<QQ", full_off, len(dir_idx)) + bytes(20)
        idx += struct.pack("<I", len(enc)) + enc + struct.pack("<I", 0)
        body = data + dir_idx + idx
        index_off, index_size = primary_off, len(idx)
    else:
        body = data + idx
        index_off, index_size = base_index_off, len(idx)
    foot = b""
    if version >= 7:
        foot += bytes(16)
    if version >= 4:
        foot += b"\0"
    foot += struct.pack("<IIQQ", 0x5A6F12E1, version, index_off, index_size) + bytes(20)
    if version == 9:
        foot += b"\0"
    if version >= 8:
        foot += b"".join(z(n, 32) for n in (comp_names + ["", "", ""])[:names_count])
    return body + foot


def ser_entry(version, rec, sha):
    e = struct.pack("<QQQ", rec["off"], rec["size"], rec["usize"]) + struct.pack("<I", rec["method"])
    if version <= 1:
        e += bytes(8)
    e += sha
    if version >= 3:
        if rec["method"]:
            e += struct.pack("<I", len(rec["blocks"])) + b"".join(struct.pack("<QQ", a, b) for a, b in rec["blocks"])
        e += b"\0" + struct.pack("<I", rec["bs"])
    return e


def sfl():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    nt, noffs = b"", []
    for n in names:
        noffs.append(len(nt))
        nt += n.encode() + b"\0"
    files_off = 29
    names_off = files_off + 17 * 3
    data_off = names_off + len(nt)
    off, fl, data = data_off, b"", b""
    for i, (no, p) in enumerate(zip(noffs, PAYLOADS)):
        fl += struct.pack("<IBIII", no, 0, i + 10, off, len(p))
        data += p
        off += len(p)
    return b"SFL10" + struct.pack("<IIIIII", 29, files_off, names_off, 0, 3, len(nt)) + fl + nt + data


def bw2stuff():
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    data, off, ents = b"", 0, b""
    for n, p in zip(names, PAYLOADS):
        ents += z(n, 256) + struct.pack("<III", off, len(p), 123)
        data += p
        off += len(p)
    return data + ents + struct.pack("<I", len(data))


def nightfire(version):
    import zlib
    z1 = zlib.compress(PAYLOADS[1])
    def rec(name, comp, size, csize):
        return struct.pack("<I", len(name)) + name.encode() + bytes([comp]) + struct.pack("<II", size, csize)
    items = [("empty.dat", 0, 0, 0, b""), ("hello.txt", 0, len(PAYLOADS[0]), len(PAYLOADS[0]), PAYLOADS[0]),
             ("data.bin", 1, len(PAYLOADS[1]), len(z1), z1)]
    head = struct.pack("<iii", 1, version, 0) + b"ROOT" + struct.pack("<i", 1) + (struct.pack("<i", 3) if version == 3 else b"")
    tree, data = b"", b""
    def put(nm, comp, sz, csz, body):
        nonlocal tree, data
        tree += rec(nm, comp, sz, csz)
        if version == 1:
            tree += body
        else:
            data += body
    put(*items[0])
    # folder "data" with 2 files, ended by a 0
    tree += struct.pack("<I", 0) + struct.pack("<I", 4) + b"data" + struct.pack("<i", 0) + (struct.pack("<i", 2) if version == 3 else b"")
    put(*items[1])
    put(*items[2])
    tree += struct.pack("<I", 0)
    return head + tree + (data if version == 3 else b"")


def edat():
    data = b"".join(PAYLOADS)
    base = 33
    offs = [base, base + len(PAYLOADS[0]), base + len(PAYLOADS[0]) + len(PAYLOADS[1])]
    def fil(last, off, size):
        return struct.pack("<iiiB", last, off, size, 0)
    def name(n):
        return n.encode() + b"\0"
    group = b""
    # hello.txt (not the last of the group)
    hello_at = len(group)
    group += struct.pack("<i", 0) + fil(1, offs[0], len(PAYLOADS[0])) + name("hello.txt")
    if (len("hello.txt") + 2) % 2: group += b"\0"
    # a file sharing the name of hello.txt: the indicator is the (negative) distance back to it
    here = len(group)
    name_pos = hello_at + 4 + 13
    group += struct.pack("<i", name_pos - here) + fil(1, offs[0], len(PAYLOADS[0])) + b"\0"
    group += struct.pack("<i", 0) + fil(0, offs[1], len(PAYLOADS[1])) + name("data.bin")
    if (len("data.bin") + 2) % 2: group += b"\0"
    d = struct.pack("<i", 0) + fil(1, offs[2], 0) + name("empty.dat")
    if (len("empty.dat") + 2) % 2: d += b"\0"
    gname = name("data")
    pad = b"\0" if (len("data") + 1) % 2 else b""
    d += struct.pack("<ii", 1, len(group)) + gname + pad + group
    # the "negative" indicator is relative to the start of the whole directory buffer
    shift = len(d) - len(group)
    group2 = bytearray(group)
    # re-derive: rebuild with absolute positions
    group = b""
    hello_at = shift + len(group)
    group += struct.pack("<i", 0) + fil(1, offs[0], len(PAYLOADS[0])) + name("hello.txt")
    if (len("hello.txt") + 2) % 2: group += b"\0"
    here = shift + len(group)
    name_pos = hello_at + 4 + 13
    group += struct.pack("<i", name_pos - here) + fil(1, offs[0], len(PAYLOADS[0])) + b"\0"
    group += struct.pack("<i", 0) + fil(0, offs[1], len(PAYLOADS[1])) + name("data.bin")
    if (len("data.bin") + 2) % 2: group += b"\0"
    d = struct.pack("<i", 0) + fil(1, offs[2], 0) + name("empty.dat")
    if (len("empty.dat") + 2) % 2: d += b"\0"
    d += struct.pack("<ii", 1, len(group)) + gname + pad + group
    head = b"edat" + struct.pack("<iiiii", 0x12, 20050204, 0, 0, 65536) + b"\0" + struct.pack("<II", base + len(data), len(d))
    assert len(head) == 33
    return head + data + d


def ubibf():
    # "hello.txt" and "data.bin" in the folder "data", "empty.dat" in the root
    def files(items):
        return b"\0" + struct.pack("<I", len(items)) + b"".join(struct.pack("<I", len(n)) + n.encode() + struct.pack("<II", sz, off) for n, sz, off in items)
    head = struct.pack("<I", 11) + b"UBI_BF_SIG\0" + struct.pack("<ii", 1, 0)
    # sizes of the directory are computed with a first pass
    def build(base):
        off = base
        it = []
        for n, p in (("hello.txt", PAYLOADS[0]), ("data.bin", PAYLOADS[1])):
            it.append((n, len(p), off)); off += len(p)
        sub = files(it)
        root = bytes([1]) + struct.pack("<I", 4) + b"data" + sub + struct.pack("<I", 0)
        return root, off
    root, _ = build(0)
    base = len(head) + len(root)
    root, off = build(base)
    return head + root + PAYLOADS[0] + PAYLOADS[1]


def adf():
    mp3 = bytes([0xFF, 0xFB, 0x90, 0x00]) + HELLO * 3
    return bytes(b ^ 0x22 for b in mp3)


def lz4_compress(data):
    """Plain LZ4 block compressor (greedy, 4-byte hash of the last position)."""
    out, lit_start, i, table = bytearray(), 0, 0, {}
    n = len(data)
    def emit(lit, mlen, off):
        tok_l = min(len(lit), 15)
        out.append(tok_l << 4 | (min(mlen - 4, 15) if mlen else 0))
        if len(lit) >= 15:
            r = len(lit) - 15
            while r >= 255: out.append(255); r -= 255
            out.append(r)
        out.extend(lit)
        if mlen:
            out.extend(struct.pack("<H", off))
            if mlen - 4 >= 15:
                r = mlen - 4 - 15
                while r >= 255: out.append(255); r -= 255
                out.append(r)
    while i + 12 <= n:
        key = data[i:i + 4]
        cand = table.get(key)
        table[key] = i
        if cand is not None and i - cand < 65536:
            m = 4
            while i + m < n - 5 and data[cand + m] == data[i + m] and m < 100000:
                m += 1
            emit(data[lit_start:i], m, i - cand)
            i += m
            lit_start = i
        else:
            i += 1
    emit(data[lit_start:], 0, 0)
    return bytes(out)


def unityfs(version, info_comp, block_comp, info_at_end=False, files=None):
    """files: [(name, flags, data)] (default: the 3 payloads, the first one flagged as a serialized file)"""
    import lzma
    if files is None:
        stream = b"".join(PAYLOADS)
        nodes = [("CAB-test/hello.txt", 0, len(PAYLOADS[0]), 4), ("CAB-test/data.bin", len(PAYLOADS[0]), len(PAYLOADS[1]), 0),
                 ("CAB-test.resS", len(PAYLOADS[0]) + len(PAYLOADS[1]), 0, 0)]
    else:
        stream, nodes = b"", []
        for n, fl, b in files:
            nodes.append((n, len(stream), len(b), fl))
            stream += b
    def comp(method, b):
        if method == 0: return b
        if method == 1:
            a = lzma.compress(b, format=lzma.FORMAT_ALONE, filters=[{"id": lzma.FILTER_LZMA1, "preset": 3, "lc": 3, "lp": 0, "pb": 2}])
            return a[:5] + a[13:]
        return lz4_compress(b)
    if block_comp == 1:
        chunks = [stream]
    else:
        chunks = [stream[i:i + 65536] for i in range(0, len(stream), 65536)]
    blocks, datab = [], b""
    for c in chunks:
        pc = comp(block_comp, c)
        if block_comp and len(pc) >= len(c) and block_comp != 1:
            blocks.append((len(c), len(c), 0)); datab += c
        else:
            blocks.append((len(c), len(pc), block_comp)); datab += pc
    info = bytes(16) + struct.pack(">I", len(blocks)) + b"".join(struct.pack(">IIH", u, c, f) for u, c, f in blocks)
    info += struct.pack(">I", len(nodes)) + b"".join(struct.pack(">qqI", o, sz, fl) + n.encode() + b"\0" for n, o, sz, fl in nodes)
    cinfo = comp(info_comp, info)
    flags = info_comp | 0x40 | (0x80 if info_at_end else 0) | (0x200 if version == 7 else 0)
    hdr0 = b"UnityFS\0" + struct.pack(">I", version) + b"5.x.x\0" + b"2020.3.0f1\0"
    total = 0
    body = hdr0 + struct.pack(">qIII", total, len(cinfo), len(info), flags)
    if version >= 7:
        body += bytes((-len(body)) % 16)
    if not info_at_end:
        body += cinfo
        if version == 7:
            body += bytes((-len(body)) % 16)
    body += datab
    if info_at_end:
        body += cinfo
    return body[:len(hdr0)] + struct.pack(">q", len(body)) + body[len(hdr0) + 8:]


# ---- Unity serialized files (the CAB-* files of the bundles) with type trees ----

def _unity_common_names():
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import unity_ref
    names = unity_ref.COMMON.split(b"\0")[:-1]
    offs, p = {}, 0
    for n in names:
        offs[n] = p
        p += len(n) + 1
    return offs

def _str_nodes(level, name):
    return [(level, "string", name, -1, 0x4000), (level + 1, "Array", "Array", -1, 0x4000),
            (level + 2, "int", "size", 4, 0), (level + 2, "char", "data", 1, 1)]

USF_TREES = {
    28: [(0, "Texture2D", "Base", -1, 0)] + _str_nodes(1, "m_Name") + [
        (1, "int", "m_Width", 4, 0), (1, "int", "m_Height", 4, 0), (1, "int", "m_CompleteImageSize", 4, 0),
        (1, "int", "m_TextureFormat", 4, 0), (1, "TypelessData", "image data", -1, 0x4000), (2, "int", "size", 4, 0),
        (2, "UInt8", "data", 1, 0), (1, "StreamingInfo", "m_StreamData", -1, 0), (2, "UInt64", "offset", 8, 0),
        (2, "unsigned int", "size", 4, 0)] + _str_nodes(2, "path"),
    49: [(0, "TextAsset", "Base", -1, 0)] + _str_nodes(1, "m_Name") + _str_nodes(1, "m_Script"),
    83: [(0, "AudioClip", "Base", -1, 0)] + _str_nodes(1, "m_Name") + [(1, "int", "m_Channels", 4, 0),
        (1, "StreamedResource", "m_Resource", -1, 0)] + _str_nodes(2, "m_Source") + [
        (2, "UInt64", "m_Offset", 8, 0), (2, "UInt64", "m_Size", 8, 0), (1, "int", "m_CompressionFormat", 4, 0)],
    128: [(0, "Font", "Base", -1, 0)] + _str_nodes(1, "m_Name") + [(1, "vector", "m_FontData", -1, 0x4000),
        (2, "Array", "Array", -1, 0x4000), (3, "int", "size", 4, 0), (3, "UInt8", "data", 1, 0)],
    1: [(0, "GameObject", "Base", -1, 0)] + _str_nodes(1, "m_Name"),
}

class _Obj:
    """Serialized data of one object (alignment is relative to its start)"""
    def __init__(self): self.b = bytearray()
    def align(self): self.b += bytes((-len(self.b)) % 4)
    def i32(self, v): self.b += struct.pack("<i", v)
    def u32(self, v): self.b += struct.pack("<I", v)
    def u64(self, v): self.b += struct.pack("<Q", v)
    def string(self, v):
        self.u32(len(v)); self.b += v; self.align()
    def result(self): return bytes(self.b)

def usf_texture(name, fmt, w, h, image=b"", stream=None):
    """stream = (path, offset, size)"""
    o = _Obj(); o.string(name); o.i32(w); o.i32(h); o.i32(len(image) or (stream[2] if stream else 0)); o.i32(fmt)
    o.i32(len(image)); o.b += image; o.align()
    o.u64(stream[1] if stream else 0); o.u32(stream[2] if stream else 0); o.string(stream[0] if stream else b"")
    return o.result()

def usf_text(name, script):
    o = _Obj(); o.string(name); o.string(script); return o.result()

def usf_audio(name, source, offset, size):
    o = _Obj(); o.string(name); o.i32(2); o.string(source); o.u64(offset); o.u64(size); o.i32(1); return o.result()

def usf_font(name, data):
    o = _Obj(); o.string(name); o.i32(len(data)); o.b += data; o.align(); return o.result()

def usf_gameobject(name):
    o = _Obj(); o.string(name); return o.result()

def serialized_file(version, objs, tree=True, common=False):
    """objs: [(path_id, class_id, data)]"""
    cn = _unity_common_names() if common else {}
    classes = []
    for _, c, _ in objs:
        if c not in classes: classes.append(c)
    meta = (b"2018.4.0f1\0" if version < 22 else b"2022.3.0f1\0") + struct.pack("<i", 19) + bytes([1 if tree else 0])
    meta += struct.pack("<i", len(classes))
    for c in classes:
        meta += struct.pack("<ibh", c, 0, -1) + bytes(16)
        if tree:
            nodes = USF_TREES[c] if c in USF_TREES else _real_tree(c)
            strtab, offs = b"", {}
            def off(t):
                tb = t.encode()
                if tb in cn: return 0x80000000 | cn[tb]
                if tb not in offs:
                    offs[tb] = len(strtab_holder[0]); strtab_holder[0] += tb + b"\0"
                return offs[tb]
            strtab_holder = [b""]
            blob = b""
            for i, (lvl, ty, nm, sz, fl) in enumerate(nodes):
                blob += struct.pack("<HBBIIiii", 1, lvl, 0, off(ty), off(nm), sz, i, fl) + (bytes(8) if version >= 19 else b"")
            meta += struct.pack("<II", len(nodes), len(strtab_holder[0])) + blob + strtab_holder[0]
            if version >= 21: meta += struct.pack("<i", 0)      # type dependencies: only with the tree
    meta += struct.pack("<i", len(objs))
    data, entries = b"", []
    for pid, c, d in objs:
        data += bytes((-len(data)) % 8)
        entries.append((pid, len(data), len(d), classes.index(c)))
        data += d
    for pid, start, size, ti in entries:
        meta += bytes((-(len(meta) + (48 if version >= 22 else 20))) % 4)      # aligned in the file, the header is 20 or 48 bytes
        meta += struct.pack("<q", pid) + (struct.pack("<q", start) if version >= 22 else struct.pack("<I", start)) + struct.pack("<Ii", size, ti)
    hlen = 48 if version >= 22 else 20
    doff = (hlen + len(meta) + 15) // 16 * 16
    total = doff + len(data)
    if version >= 22:
        head = struct.pack(">IIII", 0, 0, version, 0) + bytes(4) + struct.pack(">Iqqq", 0, total, doff, 0)
    else:
        head = struct.pack(">IIII", doff - hlen, total, version, doff) + bytes(4)
    return head + meta + bytes(doff - hlen - len(meta)) + data

def unity_objects_fixture(version, common=False):
    tex = lambda *a, **k: usf_texture(*a, **k)
    ress = bytes(range(16)) + bytes((i * 5 + 1) & 255 for i in range(40)) + bytes(8)
    fsb = bytes(8) + b"FSB5" + bytes(range(60))
    objs = [
        (1, 28, tex(b"rgba", 4, 2, 2, bytes([255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 128]))),
        (7, 28, tex(b"rgba", 5, 2, 1, bytes([1, 2, 3, 4, 5, 6, 7, 8]))),
        (2, 28, tex(b"dxt1", 10, 8, 8, stream=(b"archive:/CAB-obj/CAB-obj.resS", 16, 40))),
        (3, 28, tex(b"alpha8", 1, 4, 4, bytes(range(16)))),
        (4, 28, tex(b"rgb24", 3, 2, 1, bytes([10, 20, 30, 40, 50, 60]))),
        (5, 28, tex(b"bc7", 25, 4, 4, bytes(range(100, 116)))),
        (6, 28, tex(b"etc2", 47, 4, 4, bytes(range(8)))),
        (8, 28, tex(b"short", 4, 100, 100, bytes(16))),
        (9, 28, tex(b"empty", 4, 4, 4)),
        (10, 83, usf_audio(b"clip", b"archive:/CAB-obj/CAB-obj.resource", 8, 64)),
        (11, 49, usf_text(b"json", b'{"a": 1}')),
        (12, 49, usf_text(b"raw", bytes(range(8)))),
        (13, 49, usf_text(b"a/b", b"hello")),
        (14, 49, usf_text(b"", b"<x/>")),
        (15, 128, usf_font(b"font", b"OTTO" + bytes(20))),
        (16, 1, usf_gameobject(b"ignored")),
    ]
    files = [("CAB-obj", 4, serialized_file(version, objs, True, common)), ("CAB-obj.resS", 0, ress),
             ("CAB-obj.resource", 0, fsb), ("CAB-stripped", 4, serialized_file(version, objs[:2], False))]
    return files

def _template_object(tree, vals):
    """Serialized data of an object laid out as the type tree `tree` ([level, type, name, size, flags]
    rows of unity_trees.json); vals maps field paths to numbers or bytes, missing fields are zero"""
    o = _Obj()
    vals = dict(vals)
    for key, val in list(vals.items()):                # lists of dicts (arrays of structures) as Name[k].field
        if isinstance(val, list) and val and isinstance(val[0], dict):
            for k, d in enumerate(val):
                for f, v in d.items(): vals["%s[%d].%s" % (key, k, f)] = v
    def end(i):
        j = i + 1
        while j < len(tree) and tree[j][0] > tree[i][0]: j += 1
        return j
    def put(i, path):
        lvl, ty, nm, sz, fl = tree[i]
        e = end(i)
        if ty in ("Array", "TypelessData") and e - i == 3:
            key = path[:-6] if ty == "Array" and path.endswith(".Array") else path
            v = vals.get(key, b"")
            es = tree[i + 2][3]
            v = v if isinstance(v, bytes) else bytes(v * es)
            o.u32(len(v) // es); o.b += v
        elif ty in ("Array", "TypelessData") and e - i > 3:
            key = path[:-6] if ty == "Array" and path.endswith(".Array") else path
            items = vals.get(key, [])                   # a list of dicts, or empty
            o.u32(len(items))
            for k in range(len(items)):
                put(i + 2, "%s[%d]" % (key, k))
        elif e == i + 1:
            o.b += int(vals.get(path, 0)).to_bytes(sz, "little", signed=False)
        else:
            j = i + 1
            while j < e:
                put(j, tree[j][2] if not path else path + "." + tree[j][2])
                j = end(j)
        if fl & 0x4000: o.align()
    put(0, "")
    return o.result()

def _real_tree(cls):
    """a real type tree of the class (the newest of unity_trees.json) as rows of the fixture builder"""
    ts = [t for t in _unity_trees() if t["cls"] == cls]
    return [tuple(r) for r in ts[-1]["tree"]]

def _unity_trees():
    import json
    return json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "unity_trees.json")))

def unity_notree_objects(template, base):
    """(version, objects, .resS, .resource) of a serialized file without type trees whose objects are laid out as the real tree number `template`
    (index among the trees of the same class in unity_trees.json); base is the path of the streams, without extension"""
    tr = _unity_trees()
    by = lambda c: [t for t in tr if t["cls"] == c]
    ser = by(28)[template]["serialized"][0]
    tx = lambda name, fmt, w, h, img=b"", stream=None: _template_object(by(28)[template]["tree"], {
        "m_Name": name, "m_Width": w, "m_Height": h, "m_TextureFormat": fmt,
        "m_CompleteImageSize": len(img) + (stream[2] if stream else 0), "image data": img,
        "m_StreamData.offset": stream[1] if stream else 0, "m_StreamData.size": stream[2] if stream else 0,
        "m_StreamData.path": stream[0] if stream else b""})
    k = min(template, len(by(83)) - 1)
    objs = [
        (1, 28, tx(b"rgba", 4, 2, 2, bytes(range(16)))),
        (2, 28, tx(b"dxt5", 12, 8, 8, stream=(b"archive:/" + base + b".resS", 8, 64))),
        (3, 28, tx(b"alpha", 1, 4, 4, bytes(range(16)))),
        (4, 49, _template_object(by(49)[0]["tree"], {"m_Name": b"cfg", "m_Script": b'{"k": [1, 2]}'})),
        (5, 83, _template_object(by(83)[k]["tree"], {"m_Name": b"snd", "m_Channels": 2, "m_Resource.m_Source": b"archive:/" + base + b".resource",
                                                      "m_Resource.m_Offset": 4, "m_Resource.m_Size": 40})),
        (6, 128, _template_object(by(128)[0]["tree"], {"m_Name": b"fnt", "m_FontData": b"OTTO" + bytes(11)})),
        (7, 1, usf_gameobject(b"ignored")),
    ]
    ress = bytes((i * 3) & 255 for i in range(80))
    fsb = bytes(4) + b"FSB5" + bytes(range(48))
    return ser, objs, ress, fsb

def unity_notree_fixture(template):
    ser, objs, ress, fsb = unity_notree_objects(template, b"CAB-nt/CAB-nt")
    return [("CAB-nt", 4, serialized_file(ser, objs, False)), ("CAB-nt.resS", 0, ress), ("CAB-nt.resource", 0, fsb)]

def unity_assets_fixtures():
    """a stand-alone serialized file of a player (no trees, v21 layout) and its data streams"""
    ser, objs, ress, fsb = unity_notree_objects(1, b"test.assets")
    blob = serialized_file(ser, objs, False)
    return [("test.assets", blob), ("test.assets.resS", ress), ("test.assets.resource", fsb), ("level7", blob)]

def fsb5(mode, samples, names=True):
    """samples: [(name, data, freq_index, channels, real_freq or None[, number of samples[, vorbis setup crc]])]; the data
    is placed in the reverse order in the file (positions are not increasing in the banks of FMOD Studio)"""
    heads, blobs, off = b"", {}, 0
    order = list(range(len(samples)))[::-1]
    for i in order:
        blobs[i] = off
        off += (len(samples[i][1]) + 31) // 32 * 32
    data = b"".join(samples[i][1] + bytes(-len(samples[i][1]) % 32) for i in order)
    bps = {1: 1, 2: 2, 3: 3, 4: 4, 5: 4}
    for i, smp in enumerate(samples):
        nm, d, fi, ch, rf = smp[:5]
        count = smp[5] if len(smp) > 5 and smp[5] is not None else (len(d) // (bps[mode] * ch) if mode in bps else len(d))
        crc = smp[6] if len(smp) > 6 else None
        chunks = []
        if rf: chunks.append((2, struct.pack("<I", rf)))
        if crc is not None: chunks.append((11, struct.pack("<II", crc, 0)))
        chcode = {1: 0, 2: 1, 6: 2, 8: 3}[ch]
        bits = (1 if chunks else 0) | (fi << 1) | (chcode << 5) | ((blobs[i] // 32) << 7) | (count << 34)
        heads += struct.pack("<Q", bits)
        for k, (ty, payload) in enumerate(chunks):
            heads += struct.pack("<I", (1 if k < len(chunks) - 1 else 0) | (len(payload) << 1) | (ty << 25)) + payload
    nt = b""
    if names:
        base, strs = 4 * len(samples), b""
        for smp in samples:
            nt += struct.pack("<I", base + len(strs)); strs += smp[0] + b"\0"
        nt += strs
        nt += bytes(-len(nt) % 4)
    return (b"FSB5" + struct.pack("<6I", 1, len(samples), len(heads), len(nt), len(data), mode) + bytes(32) + heads + nt + data)

def fmod_bank(fsb):
    snd = bytes(24) + fsb
    ch = lambda t, b: t + struct.pack("<I", len(b)) + b + bytes(len(b) & 1)
    body = b"FEV " + ch(b"FMT ", struct.pack("<I", 0x40)) + ch(b"LIST", b"PROJ" + bytes(11)) + ch(b"SND ", snd)
    return b"RIFF" + struct.pack("<I", len(body)) + body

FSB_SAMPLES = [(b"boom", bytes(range(1, 41)), 8, 2, None), (b"zap/x", bytes((i * 7) & 255 for i in range(33)), 3, 1, 22050), (b"boom", b"\x01\x02\x03\x04", 9, 1, None)]

def _mpeg_frames(n, pad=4):
    """n Layer III frames (MPEG 1, 44100 Hz, 128 kbit/s: 417 bytes) each padded to a multiple of pad, then zeros"""
    fr = b"\xff\xfb\x90\x00" + bytes((i * 7) & 255 for i in range(413))
    return b"".join(fr + bytes(-len(fr) % pad) for _ in range(n)) + bytes(5)

def _vorbis_packets(sizes, mode_bits=1, seed=1):
    """FSB Vorbis data: 16 bits size, audio packet (first bit 0, then the mode), zero size to end"""
    import random
    r = random.Random(seed)
    out = b""
    for i, n in enumerate(sizes):
        pk = bytearray(r.getrandbits(8) for _ in range(n))
        pk[0] = (pk[0] & 0xF0) | ((i % 2) << 1)           # audio packet, mode i % 2
        out += struct.pack("<H", n) + bytes(pk)
    return out + struct.pack("<H", 0)

VORBIS_CRC = 0xA72297FF          # a setup packet of the table (2 modes: short and long)
VORBIS_SAMPLES = [(b"voice", _vorbis_packets([60, 130, 77, 300, 20, 99]), 9, 2, None, 4000, VORBIS_CRC),
                  (b"unknown", _vorbis_packets([40, 41], seed=2), 9, 2, None, 900, 0x12345678),
                  (b"voice2", _vorbis_packets([500] * 120, seed=3), 8, 2, None, 100000, VORBIS_CRC)]

def _xnb_content(reader, obj):
    r7 = lambda n: bytes([n]) if n < 128 else bytes([n & 127 | 128, n >> 7])
    nm = reader.encode()
    return r7(1) + r7(len(nm)) + nm + struct.pack("<i", 0) + r7(0) + r7(1) + obj

def _xnb(content, comp=0):
    """comp: 0 none, 1 LZ4 (flag 0x40), 2 LZX made of uncompressed blocks (flag 0x80)"""
    if comp == 0:
        return b"XNBw" + bytes([5, 0]) + struct.pack("<I", 10 + len(content)) + content
    if comp == 1:
        lit = len(content)
        blk = bytes([min(lit, 15) << 4]) + (bytes([255] * ((lit - 15) // 255) + [(lit - 15) % 255]) if lit >= 15 else b"") + content
        return b"XNBd" + bytes([5, 0x40]) + struct.pack("<I", 14 + len(blk)) + struct.pack("<I", len(content)) + blk
    # LZX: one block of type "uncompressed" over all the content, chunks of 32768 bytes of output
    bits = []
    def put(v, n): bits.extend((v >> (n - 1 - i)) & 1 for i in range(n))
    put(0, 1); put(3, 3); put(len(content) >> 8, 16); put(len(content) & 255, 8)
    put(0, (-len(bits)) % 16 or 16)
    w = bytearray()
    for i in range(0, len(bits), 16):
        v = int("".join(map(str, bits[i:i + 16])), 2); w += struct.pack("<H", v)
    stream = bytes(w) + struct.pack("<3I", 1, 1, 1) + content + (b"\0" if len(content) & 1 else b"")
    out, pos = b"", 0
    first = True
    chunks = [stream[:0]]
    # the first chunk holds the header and R0..R2 then data; the next chunks only data
    hdrlen = len(w) + 12
    outleft = len(content)
    parts, pos = [], 0
    while outleft > 0:
        n = min(32768, outleft)
        take = n + (hdrlen if not parts else 0) + (1 if (outleft == n and len(content) & 1) else 0)
        parts.append((n, stream[pos:pos + take])); pos += take; outleft -= n
    body = b"".join(b"\xff" + struct.pack(">HH", n, len(p)) + p for n, p in parts)
    return b"XNBw" + bytes([5, 0x80]) + struct.pack("<I", 14 + len(body)) + struct.pack("<I", len(content)) + body

def _xnb_font_content(w=4, h=4, glyphs=3):
    """a SpriteFont: Color atlas, glyph and cropping rectangles, characters, line spacing, spacing, kerning, default char"""
    r7 = lambda n: bytes([n]) if n < 128 else bytes([n & 127 | 128, n >> 7])
    names = ["Microsoft.Xna.Framework.Content.SpriteFontReader", "Microsoft.Xna.Framework.Content.Texture2DReader",
             "Microsoft.Xna.Framework.Content.ListReader`1[[Microsoft.Xna.Framework.Rectangle]]",
             "Microsoft.Xna.Framework.Content.ListReader`1[[System.Char]]",
             "Microsoft.Xna.Framework.Content.ListReader`1[[Microsoft.Xna.Framework.Vector3]]"]
    c = r7(len(names)) + b"".join(r7(len(n)) + n.encode() + struct.pack("<i", 0) for n in names) + r7(0) + r7(1)
    px = bytes((i * 9) & 255 for i in range(w * h * 4))
    c += r7(2) + struct.pack("<5I", 0, w, h, 1, len(px)) + px
    c += r7(3) + struct.pack("<i", glyphs) + b"".join(struct.pack("<4i", i, 1, 2, 3) for i in range(glyphs))
    c += r7(3) + struct.pack("<i", glyphs) + b"".join(struct.pack("<4i", 1, i, 4, 5) for i in range(glyphs))
    chars = [0x41, 0x42, 0xE9][:glyphs]
    c += r7(4) + struct.pack("<i", glyphs) + b"".join(chr(ch).encode("utf-8") for ch in chars)
    c += struct.pack("<if", 12, 1.0)
    c += r7(5) + struct.pack("<i", glyphs) + b"".join(struct.pack("<3f", 1.0 + i, 5.0, 2.0) for i in range(glyphs))
    c += bytes([1]) + b"?"
    return c

def dawl_fixture():
    """three entries: an XML (type 30), a texture (type 4) and an unnamed type 99 one"""
    types = [(30, 1), (4, 2), (99, 3)]
    table_start = 2048
    entries_pos = table_start + 4 + 0 + 4
    data = b"".join(PAYLOADS)
    data_start = entries_pos + 48 * 3
    ents, off = b"", data_start
    for (t, u), p in zip(types, PAYLOADS):
        ents += struct.pack("<IIIIqqqq", u, 0, t, 0, off, len(p), len(p), 0)
        off += len(p)
    head = b"DAWL" + struct.pack("<II", 1, 1)
    return head.ljust(table_start, b"\0") + struct.pack("<i", 0) + struct.pack("<i", 3) + ents + data

def dta_cnt_fixtures():
    """test.dta (the data) and test.cnt (the directory); an entry of size 0 is left out"""
    names = ["data\\hello.txt", "data\\data.bin", "empty.dat"]
    dta = b"DTA_" + b"".join(PAYLOADS)
    dta = dta + bytes(max(0, 40 - len(dta)))
    cnt = b"COMPCNT4" + struct.pack("<iiii", len(dta), 0, 0, 3)
    off = 4
    for n, p in zip(names, PAYLOADS):
        cnt += struct.pack("<ii", off, len(p)) + struct.pack("<H", len(n)) + n.encode()
        off += len(p)
    return dta, cnt

def lzw_literals(data):
    """Vietcong LZW stream made only of literal codes (the decoder still grows its dictionary and its code width)"""
    out, acc, nacc = bytearray(), 0, 0
    bits, dictsize, dictlen = 9, 257, 0
    for b in data:
        acc |= b << nacc
        nacc += bits
        while nacc >= 8:
            out.append(acc & 255)
            acc >>= 8
            nacc -= 8
        if dictlen:
            dictlen += 1
            dictsize += 1
            if (dictsize + 1) >> bits:
                bits += 1
        else:
            dictlen += 1
        dictlen = 1
    if nacc:
        out.append(acc & 255)
    return bytes(out)

CBF_KEY = bytes([0x32, 0xf3, 0x1e, 0x06, 0x45, 0x70, 0x32, 0xaa, 0x55, 0x3f, 0xf1, 0xde, 0xa3, 0x44, 0x21, 0xb4])

def cbf_fixture(crypted):
    names = ["hello.txt", "data.bin", "empty.dat"]
    body, ents = b"", []
    for i, (n, p) in enumerate(zip(names, PAYLOADS)):
        off = 64 + len(body)
        if i == 1:
            lz = lzw_literals(p)
            chunk = b"[..]" + struct.pack("<ii", len(lz), len(p)) + lz
            body += chunk
            ents.append((off, len(p), len(chunk), 1, n))
        else:
            size = len(p)
            raw = bytes((((b ^ (size & 255)) + ((90 - (size & 255)) & 255)) & 255) for b in p) if crypted else p
            body += raw
            ents.append((off, size, size, 0, n))
    diroff = 64 + len(body)
    d = b""
    for off, size, comp, ctype, n in ents:
        rec = struct.pack("<qiqqiii", off, 0, 0, size, comp, ctype, 0x24) + n.encode() + b"\0"
        if crypted:
            e2, c = len(rec), bytearray()
            for b in rec:
                x = b ^ CBF_KEY[e2 & 15]
                c.append(x)
                e2 = x
            rec = struct.pack("<H", len(rec)) + bytes(c)
        d += rec
    total = diroff + len(d)
    head = b"BIGF" + bytes([1 if crypted else 0]) + b"ZBL" + struct.pack("<qiqq", total, 3, diroff, len(d)) + bytes(28)
    return head + body + d

def xcr_fixture():
    names = ["hello.txt", "data.bin", "empty.dat"]
    start = 28 + 532 * 3
    ents, body, off = b"", b"", start
    for n, p in zip(names, PAYLOADS):
        full = ("C:\\game\\" + n) if n == "data.bin" else n
        ents += n.encode().ljust(256, b"\0") + full.encode().ljust(256, b"\0") + struct.pack("<ii", off, len(p)) + bytes(12)
        body += p
        off += len(p)
    total = start + len(body)
    return b"xcr File 1.00" + bytes(7) + struct.pack("<ii", 3, total) + ents + body

def sdt_fixture():
    names = ["hello", "data", "empty"]
    table = struct.pack("<I", 3)
    recs, base = b"", 4 + 12
    for n, p in zip(names, PAYLOADS):
        table += struct.pack("<I", base + len(recs))
        recs += struct.pack("<ii16s4i", 40, len(p), n.encode(), 0, 0, 0, 0) + p
    return table + recs

def star_crusader_fixture():
    names = ["HELLO", "DATA", "EMPTY"]
    data = b"".join(PAYLOADS)
    dir_offset = 6 + len(data)
    ents, off = b"", 6
    for n, p in zip(names, PAYLOADS):
        ents += struct.pack("<I8s", off, n.encode().ljust(8, b"\0"))
        off += len(p)
    return struct.pack("<HI", 3, dir_offset) + data + ents

def gtex_fixture():
    """a Glacier texture pack: entries (36 bytes header + name + data), then the index of 2048 slots and a second table"""
    names, types = ["tex/hello", "data", "empty"], [b"TINP", b"1TXD", b"AGBR"]
    body, offs = b"", []
    for n, t, p in zip(names, types, PAYLOADS):
        off = 16 + len(body)
        rec = n.encode() + b"\0" + p
        offs.append(off)
        body += struct.pack("<I4s4sIHH4I", 36 + len(rec), t, t, 0, 0, 0, 0, 0, 0, 0) + rec
    index = 16 + len(body)
    slots = struct.pack("<3I", *offs) + bytes(4 * (2048 - 3))
    return struct.pack("<4I", index, index + 0x2000, 3, 4) + body + slots + bytes(0x2000)

def dni_fixture():
    import zlib
    z = zlib.compress(PAYLOADS[1])
    root_at = 28
    sub_at = root_at + 8 + 3 * 4
    ft = sub_at + 8 + 4
    # names
    names = b"\0hello.txt\0empty.dat\0sub\0data.bin\0"
    n_empty, n_hello, n_emptydat, n_sub, n_data = 0, 1, 11, 21, 25
    names_at = ft + 20 * 3
    data_at = names_at + len(names)
    off_hello = data_at
    off_data = off_hello + len(PAYLOADS[0])
    off_emptydat = off_data + len(z)
    rec = lambda name, size, off, comp: struct.pack("<5i", names_at + name, 0, size, off, comp)
    recs = rec(n_hello, len(PAYLOADS[0]), off_hello, 0) + rec(n_data, len(PAYLOADS[1]), off_data, len(z)) + rec(n_emptydat, 0, off_emptydat, 0)
    root = struct.pack("<2i3i", names_at + n_empty, 3, ft, sub_at, ft + 40)
    sub = struct.pack("<2ii", names_at + n_sub, 1, ft + 20)
    head = b"Dirt" + struct.pack("<6i", 65536, root_at, ft, names_at, data_at, 0)
    return head + root + sub + recs + names + PAYLOADS[0] + z

def f22_fixture():
    names = b"\x03dir\x03ext"
    name_size = len(names)
    data = b"".join(PAYLOADS)
    dir_at = 8 + len(data)
    ents, off = b"", 8
    for h, p in zip((0x01020304, 0x0A0B0C0D, 0x7F000001), PAYLOADS):
        ents += struct.pack("<Iii", h, off, len(p))
        off += len(p)
    return struct.pack("<II", dir_at, 0x50001) + data + struct.pack("<4H", 3, 1, 1, name_size) + names + ents

def lego_fixture():
    names = b"data\0hello.txt\0data.bin\0empty.dat\0"
    pos = [0, 5, 15, 24]
    data = b"".join(PAYLOADS)
    dir_at = 8 + len(data)
    items, off = b"", 8
    for p in PAYLOADS:
        items += struct.pack("<4i", off, len(p), len(p), 0)
        off += len(p)
    nodes = struct.pack("<HHi", 3, 0, pos[0]) + struct.pack("<HHi", 0, 0, pos[1]) + struct.pack("<HHi", 0xFFFF, 0, pos[2]) + struct.pack("<HHi", 0xFFFE, 0, pos[3])
    d = struct.pack("<ii", -1, 3) + items + struct.pack("<i", 4) + nodes + struct.pack("<i", len(names)) + names
    return struct.pack("<ii", dir_at, len(d)) + data + d

def jam_fixture():
    names = ["hello", "sub", "data", "empty"]
    exts = ["txt", "bin", "dat"]
    n_root, n_sub = 2, 2
    tables = 32 + 8 * len(names) + 4 * len(exts) + 4
    root_at = tables
    sub_at = root_at + 8 * n_root
    first = sub_at + 8 * n_sub
    files, offs, off = b"", [], first
    for p in PAYLOADS:
        offs.append(off)
        files += struct.pack("<8I", len(p), len(p), 0, 0, 0, 0, 0, 0) + p
        off += 32 + len(p)
    head = b"JAM2" + struct.pack("<iI", 0, first) + b"none".ljust(16, b"\0") + struct.pack("<HH", len(names), len(exts))
    body = b"".join(n.encode().ljust(8, b"\0") for n in names) + b"".join(e.encode().ljust(4, b"\0") for e in exts)
    body += struct.pack("<HH", n_root, 0)
    root = struct.pack("<HHI", 0, 0, offs[0]) + struct.pack("<HHI", 1 | (2 << 12), 0, sub_at)
    sub = struct.pack("<HHI", 2, 1, offs[1]) + struct.pack("<HHI", 3, 2, offs[2])
    return head + body + root + sub + files

def gjd7_fixtures():
    data = b"".join(PAYLOADS)
    rl, off = b"", 0
    for n, p in zip(["HELLO.TXT", "DATA.BIN", "EMPTY.DAT"], PAYLOADS):
        rl += n.encode().ljust(12, b"\0") + struct.pack("<II", off, len(p))
        off += len(p)
    rl += b"OTHER.GJD".ljust(12, b"\0") + struct.pack("<II", 10**8, 5)   # belongs to another .GJD: left out
    return data, rl

def hog2_fixture():
    names = ["hello.txt", "data.bin", "empty.dat"]
    data_at = 68 + 48 * 3
    ents = b"".join(n.encode().ljust(36, b"\0") + struct.pack("<iii", 0, len(p), 0) for n, p in zip(names, PAYLOADS))
    return b"HOG2" + struct.pack("<ii", 3, data_at) + bytes(56) + ents + b"".join(PAYLOADS)

def bkf2_fixture():
    names = ["data\\hello.txt", "data.bin", "empty.dat"]
    data_at = 12 + 64 * 3
    ents, off = b"", data_at
    for n, p in zip(names, PAYLOADS):
        ents += n.encode().ljust(48, b"\0") + struct.pack("<II8s", off, len(p), bytes(8))
        off += len(p)
    return struct.pack("<iii", -2, 3, 48) + ents + b"".join(PAYLOADS)

def h4r_fixture():
    import zlib as _z
    def gz(b):
        c = _z.compressobj(9, _z.DEFLATED, 31)
        return c.compress(b) + c.flush()
    names = ["hello.txt", "data.bin", "empty.dat"]
    packed = [gz(PAYLOADS[0]), PAYLOADS[1], b""]
    methods = [3, 1, 3]
    strs = lambda t: struct.pack("<H", len(t)) + t
    dirlen = sum(16 + 2 + len(n) + 2 + 2 + 4 for n in names)
    pos, ents = 12 + dirlen, b""
    for n, p_, pl, m in zip(names, packed, PAYLOADS, methods):
        off = pos if p_ else 0
        ents += struct.pack("<4I", off, len(p_), len(pl) if p_ else 0, 0) + strs(n.encode()) + strs(b"") + strs(b"") + struct.pack("<I", m)
        pos += len(p_)
    return b"H4R\x05" + struct.pack("<I", 8) + struct.pack("<I", 3) + ents + b"".join(packed)

SECRET = b"R" + bytes(range(1, 200))      # starts with the first byte a .WAV has to give

def pck3_encrypt(data, key):
    out, skeleton = bytearray(data), 0
    for x in range(0, len(out), 16):
        out[x] ^= (key + skeleton) & 0xFF
        skeleton = 0 if skeleton == 240 else skeleton + 16
        key += 1
        if key == 7:
            key = 8
        if key >= 15:
            key = 0
    return bytes(out)

def pck3_fixture():
    def ent(name, flag, size, off):
        return z(name, 36) + struct.pack("<iII", flag, size, off)
    files = [("hello.txt", PAYLOADS[0]), ("data.bin", PAYLOADS[1]), ("empty.dat", PAYLOADS[2]), ("secret.wav", pck3_encrypt(SECRET, 9))]
    # root folder, hello.txt, folder sub (data.bin, empty.dat, end), secret.wav, end
    table_len = 48 * 8
    pos, offs = table_len, {}
    for n, d in files:
        offs[n] = pos
        pos += len(d)
    table = ent("", 1, 0xFFFFFFFF, 0x30)
    table += ent("hello.txt", 0, len(files[0][1]), offs["hello.txt"])
    table += ent("sub", 1, 0xFFFFFFFF, 0)
    table += ent("data.bin", 0, len(files[1][1]), offs["data.bin"])
    table += ent("empty.dat", 0, 0, offs["empty.dat"])
    table += ent("", 0xFF, 0xFFFFFFFF, 0xFFFFFFFF)
    table += ent("secret.wav", 0, len(files[3][1]), offs["secret.wav"])
    table += ent("", 0xFF, 0xFFFFFFFF, 0xFFFFFFFF)
    assert len(table) == table_len
    return table + b"".join(d for _, d in files)

def rom2res_fixture():
    def ent(unk, off, size, flags, name):
        return struct.pack("<4I", unk, off, size, flags) + z(name, 16)
    data_at = 24
    offs, pos = [], data_at
    for p_ in PAYLOADS:
        offs.append(pos)
        pos += len(p_)
    table = ent(0, offs[0], len(PAYLOADS[0]), 0, "hello.txt") + ent(0, 2, 2, 1, "sub")
    table += ent(0, offs[1], len(PAYLOADS[1]), 0, "data.bin") + ent(0, offs[2], 0, 0, "empty.dat")
    return b"&YA1" + struct.pack("<5I", 0, 2, 0, pos, len(table)) + b"".join(PAYLOADS) + table

def bagidx_fixtures():
    names = ["hello", "data", "empty"]
    media = [37, 2, 6]
    idx = b"GABA" + struct.pack("<II", 0, 3)
    off = 0
    for n, p_, m in zip(names, PAYLOADS, media):
        idx += z(n, 16) + struct.pack("<5I", off, len(p_), 22050, m, 0)
        off += len(p_)
    return b"".join(PAYLOADS), idx

def lionhead_fixture():
    names = ["C:\\Temp\\hello.txt", "C:\\Temp\\sub\\data.bin", "C:\\Temp\\empty.dat"]
    wave, ents, off = b"".join(PAYLOADS), b"", 0
    for n, p_ in zip(names, PAYLOADS):
        e = z(n, 256) + struct.pack("<3I", 0, 0, 0) + struct.pack("<2I", len(p_), off)
        ents += e.ljust(640, b"\0")
        off += len(p_)
    table = struct.pack("<HH", 3, 3) + ents
    chunk = lambda name, body: z(name, 32) + struct.pack("<I", len(body)) + body
    return b"LiOnHeAd" + chunk("LHAudioWaveData", wave) + chunk("LHAudioBankSampleTable", table)

def mrc_fixture():
    names = b""
    def name(n):
        nonlocal names
        o = len(names); names += n.encode() + b"\0"; return o
    root, sub, f0, f1, f2 = name("root"), name("sub"), name("hello.txt"), name("data.bin"), name("empty.dat")
    # dir 0: root (1 sub folder: dir 1, file 1), dir 1: sub (no sub folder, files 2 and 3)
    dlst = struct.pack("<5i", root, 1, 1, 1, 0) + struct.pack("<5i", sub, 0, 0, 2, 1)
    off, flst = 0, b""
    for n, p_ in zip((f0, f1, f2), PAYLOADS):
        flst += struct.pack("<4i", n, len(p_), off, 0)
        off += len(p_)
    blk = lambda i, b: i + struct.pack("<i", len(b)) + b
    fsys = blk(b"LIST", b"FSys" + blk(b"DLst", dlst) + blk(b"FLst", flst) + blk(b"NLst", names))
    fsto = blk(b"LIST", b"FSto" + b"".join(PAYLOADS))
    body = b"mmRC" + blk(b"FInf", struct.pack("<i", 1)) + fsys + fsto
    return b"RIFF" + struct.pack("<i", len(body)) + body

def asura_fixtures():
    def pad(n):
        return n.encode() + b"\0" * (4 - len(n) % 4)
    blk = lambda i, b: i + struct.pack("<III", 16 + len(b), 0, 0) + b
    # two resources (RSCF), the third payload in a sound list (ASTS) with absolute offsets
    body = b""
    for n, p_ in zip(["\\hello.txt", "sub\\data.bin"], PAYLOADS[:2]):
        body += blk(b"RSCF", b"\0" * 8 + struct.pack("<I", len(p_)) + pad(n) + p_)
    sounds_at = 8 + len(body)
    entry = lambda off: pad("snd\\empty.dat") + struct.pack("<II", 0, off)
    asts = blk(b"ASTS", struct.pack("<I", 1) + entry(sounds_at))
    plain = b"Asura   " + body + asts + b"\0" * 16
    z_ = __import__("zlib").compress(plain)
    packed = b"AsuraZlb" + struct.pack("<III", 0, len(z_), len(plain)) + z_
    return plain, packed

def cnc_mix_fixture():
    def td_id(name):
        name, ident, i = name.upper().encode(), 0, 0
        while i < len(name):
            a = 0
            for _ in range(4):
                a >>= 8
                if i < len(name):
                    a += name[i] << 24
                    i += 1
            ident = ((((ident << 1) | (ident >> 31)) & 0xFFFFFFFF) + a) & 0xFFFFFFFF
        return ident
    ids = [td_id("00-i008.aud"), 0x12345678, 0x9ABCDEF0]
    body, ents, off = b"", b"", 0
    for i_, p_ in zip(ids, PAYLOADS):
        ents += struct.pack("<III", i_, off, len(p_))
        body += p_
        off += len(p_)
    return struct.pack("<HI", 3, len(body)) + ents + body

def wwpak_fixture():
    names = ["empty.dat", "hello.txt", "data.bin"]
    order = [PAYLOADS[2], PAYLOADS[0], PAYLOADS[1]]
    head_len = sum(4 + len(n) + 1 for n in names) + 4
    offs, pos = [], head_len
    for p_ in order:
        offs.append(pos)
        pos += len(p_)
    head = b"".join(struct.pack("<I", o) + n.encode() + b"\0" for o, n in zip(offs, names)) + struct.pack("<I", 0)
    return head + b"".join(order)

def mos_chunk(name, nxt, data):
    return z(name, 24) + struct.pack("<i5i", nxt, *data)

def mos_xwc_fixture():
    ogg = bytes(36) + b"OggS" + b"vorbis!"                  # Enclave: 36 bytes before the Ogg stream
    blobs = PAYLOADS + [ogg]
    names = ["hello", "data", "empty", "song"]
    # file: header chunk, VERSION chunk, WAVEDATA chunk, table, data
    table_at = 48 * 3
    tab = b""
    sizes = []
    pos = table_at + 4 * 0
    # compute the size of the table first
    tab_len = sum(8 + 4 + len(n) + (-len(n)) % 4 for n in names)
    data_at = table_at + tab_len
    off = data_at
    for n, b in zip(names, blobs):
        tab += struct.pack("<ii", off, len(b)) + struct.pack("<I", len(n)) + n.encode() + bytes((-len(n)) % 4)
        off += len(b)
    out = mos_chunk("MOS DATAFILE2.0", 48, (0, 0, 0, 0, 0))
    out += mos_chunk("VERSION", 96, (0, 0, 0, 3, 0))
    out += mos_chunk("WAVEDATA", 0, (0, 0, table_at, len(names), 0))
    return out + tab + b"".join(blobs)

def mos_xtc_fixture():
    # one DXT1 texture 4x4 with 2 mip maps, Enclave layout (no extra integer)
    head = mos_chunk("MOS DATAFILE2.0", 0, (0, 0, 0, 0, 0))
    mip0 = struct.pack("<5I", 0, 0, 0, 4, 4) + struct.pack("<5I", 0, 0, 8, 0, 0) + bytes(range(8))
    base = len(head)
    table_at = base                        # offsets of the mip maps, then the mip maps
    mip0_at = table_at + 8
    mip1_at = mip0_at
    table = struct.pack("<2I", mip0_at, mip1_at)
    textures = table + mip0
    dir_at = base + len(textures)
    name = b"tex"
    entry = struct.pack("<I", len(name)) + name + bytes((-len(name)) % 4)
    entry += struct.pack("<7I", 0, 2, 512, 0, 8, 4, 4) + struct.pack("<8I", 0, table_at, 0, 0, 0, 0, 0, 0) + b"\0\0"
    directory = struct.pack("<I", 512) + bytes(52) + entry
    chunk_at = dir_at + len(directory)
    chunk = mos_chunk("TEXTURES", 0, (0, 0, 0, 0, 0)) + bytes(8) + struct.pack("<ii", dir_at, 1)
    head = mos_chunk("MOS DATAFILE2.0", chunk_at, (0, 0, 0, 0, 0))
    return head + textures + directory + chunk

def forge_fixture():
    names = ["hello.txt", "data.bin", "empty.dat"]
    data_at = 29 + 88 + 3 * 16 + 3 * 188
    dir_at = 29
    files_at = dir_at + 88
    names_at = files_at + 3 * 16
    head = b"scimitar\0" + struct.pack("<iqii", 0, dir_at, 0x10, 1)
    d = struct.pack("<8i", 3, 0, 0, 0, -1, -1, 0, 0) + struct.pack("<q", 0)
    d += struct.pack("<ii", 3, 0) + struct.pack("<q", files_at) + struct.pack("<4i", -1, -1, 0, 0) + struct.pack("<qq", names_at, 0)
    assert len(head) == 29 and len(d) == 88
    files, ents, off = b"", b"", data_at
    for n, p_ in zip(names, PAYLOADS):
        files += struct.pack("<qii", off, 0x1000 + len(p_), len(p_))
        ents += (struct.pack("<11i", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) + z(n, 128) + bytes(16))
        off += len(p_)
    return head + d + files + ents + b"".join(PAYLOADS)

def nrm_fixture():
    out = b"PakkaByRCL^DPL2000"
    for n, p_ in zip(["hello.txt", "data.bin", "empty.dat"], PAYLOADS):
        out += b"FILE\0" + struct.pack("<ii", len(n) + len(p_), len(p_)) + struct.pack("<I", len(n)) + n.encode() + p_
    return out

def florensia_fixture():
    names = ["hello.txt", "data.bin", "empty.dat"]
    off = 4 + 296 * 3
    ents = b""
    for n, p_ in zip(names, PAYLOADS):
        ents += z(n, 256) + struct.pack("<iii", 0, off, len(p_)) + bytes(28)
        off += len(p_)
    return struct.pack("<i", 3) + ents + b"".join(PAYLOADS)

def ut_package_fixture():
    def idx(v):
        neg = v < 0
        v = abs(v)
        out = [(v & 0x3F) | (0x80 if neg else 0)]
        v >>= 6
        while v:
            out[-1] |= 0x40 if len(out) == 1 else 0x80
            out.append(v & 0x7F)
            v >>= 7
        return bytes(out)
    names = ["it", "None", "Sound", "Music", "Texture", "Palette", "Package", "wav", "Group", "hello", "song", "tex", "pal", "Core", "Class"]
    n = {x: i for i, x in enumerate(names)}
    name_table = b"".join(bytes([len(x) + 1]) + x.encode() + b"\0" + struct.pack("<I", 0x70000) for x in names)
    # imports: classes the objects refer to
    imports = [("Sound",), ("Music",), ("Texture",), ("Palette",), ("Package",)]
    import_table = b"".join(idx(n["Core"]) + idx(n["Class"]) + struct.pack("<i", 0) + idx(n[c[0]]) for c in imports)
    def prop_none():
        return idx(n["None"])
    sound = prop_none() + idx(n["wav"]) + struct.pack("<I", 0) + idx(len(PAYLOADS[0])) + PAYLOADS[0]
    music = struct.pack("<H", 0) + struct.pack("<I", 0) + idx(len(PAYLOADS[1])) + PAYLOADS[1]
    colors = [0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF]       # red, green, blue, white (as r, g, b, a bytes)
    palette = prop_none() + idx(len(colors)) + b"".join(struct.pack("<I", c) for c in colors)
    pixels = bytes([0, 1, 2, 3] * 4)
    def texture(base):
        body = idx(n["Palette"]) + bytes([0x05]) + idx(4) + prop_none()      # property Palette = export 4 (the palette)
        body += bytes([1])                                                      # one mip map
        before = len(body) + 4                                                  # the dword of the position after the block
        data_at = before + len(idx(len(pixels)))
        after = base + data_at + len(pixels)
        body += struct.pack("<I", after) + idx(len(pixels)) + pixels
        body += struct.pack("<II", 4, 4) + bytes([2, 2])
        return body
    exp_count = 5
    fixed = 36 + 16 + 4
    names_at = fixed
    exports_at = names_at + len(name_table)
    # the sizes of the export records do not depend on the object offsets as long as they stay below 64: use two-byte indexes
    def export_record(cls, group, obj, size, off):
        return idx(cls) + idx(0) + struct.pack("<i", group) + idx(obj) + struct.pack("<I", 0x70004) + idx(size) + (idx(off) if size > 0 else b"")
    group_obj = b"\0"
    export_len = 0
    for _ in range(4):                      # the length of the table depends on the offsets, which depend on its length
        imports_at = exports_at + export_len
        objects_at = imports_at + len(import_table)
        offs, pos = [], objects_at
        objs = [group_obj, sound, music, palette]
        for o in objs:
            offs.append(pos)
            pos += len(o)
        tex = texture(pos)
        objs.append(tex)
        offs.append(pos)
        recs = [export_record(-5, 0, n["Group"], len(group_obj), offs[0]), export_record(-1, 1, n["hello"], len(sound), offs[1]), export_record(-2, 1, n["song"], len(music), offs[2]),
                export_record(-4, 1, n["pal"], len(palette), offs[3]), export_record(-3, 1, n["tex"], len(tex), offs[4])]
        export_table = b"".join(recs)
        export_len = len(export_table)
    imports_at = exports_at + export_len
    head = struct.pack("<IHHI", 0x9E2A83C1, 69, 0, 0) + struct.pack("<6I", len(names), names_at, exp_count, exports_at, len(imports), imports_at)
    head += bytes(16) + struct.pack("<I", 0)
    return head + name_table + export_table + import_table + b"".join(objs)

def lfd_fixture():
    res = [(b"VOIC", b"beam", b"voice data"), (b"TEXT", b"intro", b"hello"), (b"PLTT", b"empty", b"")]
    h = lambda t, n, d: t + n.ljust(8, b"\0") + struct.pack("<I", len(d))
    return h(b"RMAP", b"resource", bytes(16 * len(res))) + b"".join(h(*r) for r in res) + b"".join(h(*r) + r[2] for r in res)

def xwb_fixture():
    """a PCM 16 bits stereo wave, a MS ADPCM one, an XMA one"""
    waves = [(0 | (2 << 2) | (22050 << 5) | (4 << 23) | (1 << 31), bytes(range(40))),
             (2 | (2 << 2) | (44100 << 5) | (0 << 23), bytes((i * 3) & 255 for i in range(88))),
             (1 | (2 << 2) | (48000 << 5), b"XMAXMAXM")]
    bank = struct.pack("<II", 0x80001, len(waves)) + b"Test".ljust(64, b"\0") + struct.pack("<IIII", 24, 64, 2048, 0) + bytes(8)
    meta, data = b"", b""
    for fmt, d in waves:
        meta += struct.pack("<6I", 0, fmt, len(data), len(d), 0, 0)
        data += d
    base = 52
    seg = [(base, len(bank)), (base + len(bank), len(meta)), (base + len(bank) + len(meta), 0), (0, 0), (2048, len(data))]
    head = b"WBND" + struct.pack("<II", 46, 44) + b"".join(struct.pack("<II", *x) for x in seg)
    return (head + bank + meta).ljust(2048, b"\0") + data

def gamemaker_fixture():
    """GEN8, a SOND naming the audio 1, AUDO with a WAV and an Ogg, a TXTR with one PNG"""
    import zlib
    def chunk(tag, body): return tag + struct.pack("<I", len(body)) + body
    wav, ogg = b"RIFF" + bytes(12), b"OggS" + bytes(9)
    png = b"\x89PNG\r\n\x1a\n" + b"".join(struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b)) for t, b in ((b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)), (b"IDAT", zlib.compress(b"\0\0\0\0")), (b"IEND", b"")))
    gen8 = bytes(32)
    # layout: FORM(8) GEN8(8+32) SOND(8+8+36+4+name) AUDO TXTR
    name = b"snd_two\0"
    sond_body_len = 8 + 36 + 4 + len(name)
    p_gen8 = 8; p_sond = p_gen8 + 8 + len(gen8); p_audo = p_sond + 8 + sond_body_len
    audo_body_len = 4 + 8 + (4 + len(wav)) + (4 + len(ogg))
    p_txtr = p_audo + 8 + audo_body_len
    sond_ptr = p_sond + 8 + 8
    name_ptr = sond_ptr + 36 + 4
    sond = struct.pack("<II", 1, sond_ptr) + struct.pack("<9I", name_ptr, 0x65, 0, 0, 0, 0, 0, 0, 1) + struct.pack("<I", len(name) - 1) + name
    a0 = p_audo + 8 + 12
    a1 = a0 + 4 + len(wav)
    audo = struct.pack("<III", 2, a0, a1) + struct.pack("<I", len(wav)) + wav + struct.pack("<I", len(ogg)) + ogg
    e0 = p_txtr + 8 + 8
    txtr = struct.pack("<II", 1, e0) + struct.pack("<II", 1, e0 + 8) + png
    body = b"GEN8" + b""
    data = chunk(b"GEN8", gen8) + chunk(b"SOND", sond) + chunk(b"AUDO", audo) + chunk(b"TXTR", txtr)
    return b"FORM" + struct.pack("<I", len(data)) + data

def zip_fixture(prefix=b""):
    """deflate, stored and LZMA entries and a folder; with a prefix the offsets are those of a self extractor"""
    import zipfile, io
    b = io.BytesIO()
    with zipfile.ZipFile(b, "w") as z:
        z.writestr("dir/", b"")
        z.writestr(zipfile.ZipInfo("dir/hello.txt"), b"hello zip " * 100, zipfile.ZIP_DEFLATED)
        z.writestr(zipfile.ZipInfo("raw.bin"), bytes(range(200)), zipfile.ZIP_STORED)
        z.writestr(zipfile.ZipInfo("empty"), b"", zipfile.ZIP_STORED)
    return prefix + b.getvalue()

def xnb_fixtures():
    tex = lambda fmt, w, h, px: _xnb_content("Microsoft.Xna.Framework.Content.Texture2DReader, Microsoft.Xna.Framework.Graphics, Version=4.0.0.0", struct.pack("<5I", fmt, w, h, 1, len(px)) + px)
    px = bytes(range(16))
    snd = _xnb_content("Microsoft.Xna.Framework.Content.SoundEffectReader", struct.pack("<I", 18) + struct.pack("<HHIIHHH", 1, 1, 8000, 16000, 2, 16, 0) + struct.pack("<I", 10) + bytes(range(10)) + struct.pack("<3i", 0, 0, 5))
    big = bytes((i * 31 + i // 7) & 255 for i in range(40000))
    return [("tex.xnb", _xnb(tex(0, 2, 2, px))), ("dxt.xnb", _xnb(tex(4, 4, 4, px[:8]))), ("rgb565.xnb", _xnb(tex(1, 2, 2, px[:8]))),
            ("snd.xnb", _xnb(snd)), ("custom.xnb", _xnb(_xnb_content("My.Game.LevelDataReader, Game", b"hello world"))),
            ("lz4.xnb", _xnb(tex(0, 2, 2, px), 1)), ("lzx.xnb", _xnb(tex(0, 100, 100, big[:40000]), 2)), ("lzx_small.xnb", _xnb(tex(6, 4, 4, px), 2)),
            ("font.xnb", _xnb(_xnb_font_content())), ("font_lzx.xnb", _xnb(_xnb_font_content(), 2)), ("font_lzx_ref.xnb", _xnb(_xnb_font_content())),
            ("lzx_ref.xnb", _xnb(tex(0, 100, 100, big[:40000]))), ("lzx_small_ref.xnb", _xnb(tex(6, 4, 4, px)))]

ASTC_BLOCKS = [bytes.fromhex(h) for h in (
    "61036b6734017961fe2438335bd77499", "61e5c52d807ff23aee31993f004e1c22", "4281e389e382ff16227739f0d432b99f",
    "43a2fb9c2cdaa196b325793777c0e8ca", "4165e9e68d15cfc98e23bd5f3aeb83d2", "41445b63bbc08adc05b9afaecd5a6c36",
    "4380fb26ddc423765cfa1aa9c801d32d", "414365f2285778b753f15987f89c68ad", "426554fd20f2e1eed18ac888ae62b014",
    "61a5029b6689737646ca09fad929230d", "63c0865b0c4d0f21273240373a27d2f3", "4124b7ebc7b413b7f3f82a2943b45cbf")]

def unity_texfmts_fixture():
    """one small texture of every format the driver decodes or converts: crunched DXT1/DXT5 (made by
    crunch_enc.py), ETC1/ETC2/EAC, ASTC, 16 bits formats; the data is pseudo random but fixed"""
    import random
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import crunch_enc
    rnd = random.Random(2024)
    objs, pid = [], 1
    def add(name, fmt, w, h, data):
        nonlocal pid
        objs.append((pid, 28, usf_texture(name.encode(), fmt, w, h, data))); pid += 1
    # crunched textures: 16x12 pixels
    for fmt, cf in ((28, 0), (29, 2)):
        bx, by = 4, 3
        blocks = []
        for _ in range(bx * by):
            if cf == 0:
                blocks.append((rnd.choice([rnd.getrandbits(32) for _ in range(4)]), rnd.choice([rnd.getrandbits(32) for _ in range(5)])))
            else:
                blocks.append((rnd.getrandbits(16) & 0xFFFF, rnd.getrandbits(48), rnd.getrandbits(32), rnd.getrandbits(32)))
        add("crunch%d" % fmt, fmt, 16, 12, crunch_enc.encode(cf, 16, 12, blocks))
    add("crunch_bad", 29, 16, 12, b"Hx" + bytes(100))                # not a crunch file: stays raw
    for fmt, bb in ((34, 8), (45, 8), (46, 8), (47, 16), (41, 8), (42, 8), (43, 16), (44, 16)):
        add("etc%d" % fmt, fmt, 10, 6, bytes(rnd.getrandbits(8) for _ in range(3 * 2 * bb)))
    for fmt, bs in ((48, 4), (54, 4), (50, 6), (56, 6), (58, 10), (51, 8)):
        nb = ((12 + bs - 1) // bs) * ((10 + bs - 1) // bs)
        add("astc%d" % fmt, fmt, 12, 10, b"".join(ASTC_BLOCKS[(i * 5 + fmt) % len(ASTC_BLOCKS)] for i in range(nb)))
    # void extent blocks (a constant color)
    add("astc_void", 56, 8, 8, (bytes([0xFC, 0xFD]) + bytes(6) + bytes([0, 0x40, 0, 0x80, 0, 0xC0, 0, 0xFF])) * 4)
    for fmt in (2, 13, 7):
        add("px16_%d" % fmt, fmt, 5, 3, bytes(rnd.getrandbits(8) for _ in range(30)))
    return [("CAB-tex", 4, serialized_file(22, objs, True))]

def f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]

def unity_objs3_fixture(tree=True):
    """a texture atlas and a DXT1 one, sprites that cut them, a cubemap, two meshes; with tree=False the file has no type
    trees and every object is laid out as a real tree of unity_trees.json (the driver must find the right layout)"""
    tr = _unity_trees()
    by = lambda c: [t for t in tr if t["cls"] == c]
    ser = 22
    px = bytes((i * 11 + 5) & 255 for i in range(8 * 8 * 4))
    dxt = bytes((i * 7 + 3) & 255 for i in range(4 * 8))               # 8x8 = 2 x 2 blocks of 8 bytes
    if tree:
        tex = lambda pid, name, fmt, w, h, img: (pid, 28, usf_texture(name, fmt, w, h, img))
    else:
        t28 = by(28)[2]["tree"]
        tex = lambda pid, name, fmt, w, h, img: (pid, 28, _template_object(t28, {
            "m_Name": name, "m_Width": w, "m_Height": h, "m_TextureFormat": fmt, "m_CompleteImageSize": len(img), "image data": img}))
    def sprite(name, tex_pid, x, y, w, h):
        return _template_object(by(213)[-1]["tree"], {
            "m_Name": name, "m_Rect.width": f32(w), "m_Rect.height": f32(h), "m_PixelsToUnits": f32(100.0),
            "m_RD.texture.m_PathID": tex_pid, "m_RD.textureRect.x": f32(x), "m_RD.textureRect.y": f32(y),
            "m_RD.textureRect.width": f32(w), "m_RD.textureRect.height": f32(h)})
    cube_face = bytes((i * 3 + 1) & 255 for i in range(2 * 2 * 4))
    cube = _template_object(by(89)[-1]["tree"], {"m_Name": b"cube", "m_Width": 2, "m_Height": 2, "m_CompleteImageSize": 16, "m_TextureFormat": 4,
                                                 "m_MipCount": 1, "m_ImageCount": 6, "image data": b"".join(bytes(b ^ k for b in cube_face) for k in range(6))})
    def chans(spec):
        out = [{"stream": 0, "offset": 0, "format": 0, "dimension": 0} for _ in range(14)]
        for idx, (st, off, fmt, dim) in spec.items(): out[idx] = {"stream": st, "offset": off, "format": fmt, "dimension": dim}
        return out
    import random
    rnd = random.Random(8)
    # quad: positions + normals (stream 0, float), uv (stream 1, float), 4 vertices, 2 triangles
    vd0 = b"".join(struct.pack("<6f", x, y, 0.0, 0.0, 0.0, 1.0) for x, y in ((0, 0), (1, 0), (1, 1), (0, 1)))
    vd0 += bytes((-len(vd0)) % 16)
    vd1 = b"".join(struct.pack("<2f", u, v) for u, v in ((0, 0), (1, 0), (1, 1), (0, 1)))
    quad = _template_object(by(43)[-1]["tree"], {
        "m_Name": b"quad", "m_MeshCompression": 0, "m_IndexFormat": 0, "m_IndexBuffer": struct.pack("<6H", 0, 1, 2, 0, 2, 3),
        "m_VertexData.m_VertexCount": 4, "m_VertexData.m_DataSize": vd0 + vd1,
        "m_VertexData.m_Channels": chans({0: (0, 0, 0, 3), 1: (0, 12, 0, 3), 4: (1, 0, 0, 2)}),
        "m_SubMeshes": [{"firstByte": 0, "indexCount": 6, "topology": 0, "baseVertex": 0, "firstVertex": 0, "vertexCount": 4}]})
    # strip: half float positions with 4 components (padded), 32 bits indexes, a base vertex, no normal and no uv
    hv = b"".join(struct.pack("<4e", rnd.uniform(-2, 2), rnd.uniform(-2, 2), rnd.uniform(-2, 2), 1.0) for _ in range(6))
    strip = _template_object(by(43)[-1]["tree"], {
        "m_Name": b"strip", "m_MeshCompression": 0, "m_IndexFormat": 1, "m_IndexBuffer": struct.pack("<5I", 0, 1, 2, 3, 2),
        "m_VertexData.m_VertexCount": 6, "m_VertexData.m_DataSize": hv,
        "m_VertexData.m_Channels": chans({0: (0, 0, 1, 4)}),
        "m_SubMeshes": [{"firstByte": 0, "indexCount": 5, "topology": 1, "baseVertex": 1, "firstVertex": 1, "vertexCount": 5}]})
    objs = [tex(1, b"atlas", 4, 8, 8, px), tex(2, b"blocks", 10, 8, 8, dxt),
            (3, 213, sprite(b"spr_a", 1, 2, 1, 4, 3)), (4, 213, sprite(b"spr_b", 2, 4, 4, 4, 4)), (5, 213, sprite(b"spr_ext", 99, 0, 0, 2, 2)),
            (6, 89, cube), (7, 43, quad), (8, 43, strip)]
    return [("CAB-o3", 4, serialized_file(ser, objs, tree))]

def unity_harness_bundle():
    objs = [(1, 49, usf_text(b"hello", HELLO)), (2, 28, usf_texture(b"data", 34, 320, 240, stream=(b"archive:/CAB-obj/CAB-obj.resS", 0, len(DATA)))),
            (3, 1, usf_gameobject(b"x"))]
    return [("CAB-obj", 4, serialized_file(22, objs, True, True)), ("CAB-obj.resS", 0, DATA)]


RNC_PAYLOAD += bytes(range(256)) * 20 + b"\0" * 500 + bytes((i * 7 + i // 3) & 255 for i in range(3000))

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
for name, blob in [("test.pak", pak()), ("test.iwad", iwad()), ("test.pwad", iwad(b"PWAD")),
                   ("test.wad2", wad2()), ("test.gob", gob()), ("test.grp", grp()), ("test.spe", spe()), ("test.dpa", dpa()), ("test.rff", rff()), ("test.art", art()),
                   ("test.far", far()), ("test.vp", vp()), ("test.erf", erf()),
                   ("test.hog", hog()), ("test.big", bigf()), ("test.drs", drs()),
                   ("aoe3.bar", bar_aoe3()), ("aom.bar", bar_aom()),
                   ("test.hal", hal()), ("test.bun", bun()), ("test.bkf", bkf()), ("test.dat", gunlok()),
                   ("dups.wad", dups_wad()), ("test.sin", sin()), ("count.sin", sin(True)), ("test.snd", h3snd()),
                   ("test.pff", pff3()), ("nascar.dat", nascar()), ("sf.pak", spellforce()), ("test.zfs", zfs()), ("test.sni", sni()), ("test.pbo", pbo()), ("bf.pak", bfpak()), ("indy.gob", indy3d()), ("test.dwfb", dwfb()),
                   ("pp.res", ppres()), ("test.awf", awf()), ("test.cub", cub()), ("zan.pak", zanzarah()), ("kapl.pak", kapl()), ("test.sak", sak()), ("test.slf", slf()), ("avp.rff", rffl()), ("test.prm", prm()), ("cb.bin", cbin()), ("test.bsa", bsa()), ("tlj_test.pak", tljpak()), ("test.syn", syn()), ("test.vol", vol()), ("test.pod", pod1()), ("test.cpr", cpr()), ("test.fpk", fpk()), ("test.rez", rez()), ("test.box", lembox()), ("test.cob", cob()), ("ei.res", eires()), ("test.bnt", bnt2()), ("test.pkr", pkr()), ("pod2.pod", pod2()), ("test.sqh", sqh()), ("fable.big", fableb()), ("fear.arch00", ltar()), ("test.bag", bag()), ("test.opk", opk()), ("test.oaf", oaf()), ("sw.pak", europa()), ("tm.pak", tmpak()), ("test.ssa", ssa()), ("test.dzip", dzip()), ("pod3.pod", pod3()), ("pod5.pod", pod3(True)), ("test.gzp", gzp()), ("test.rcf", rcf()), ("test.mtf", mtf()), ("retail.rfa", rfa()), ("beta.rfa", rfa(True)), ("test.vfs", vfs()), ("test.hpi", hpi()), ("plain.ufo", hpi(0)), ("painkiller.pak", pkpak()), ("test.xrs", ppxrs()), ("test.stuff", eve_stuff()), ("v1.tgp", tgp(1)), ("v2.tgp", tgp(2)), ("tonga.pak", tonga()), ("v1.pck", godot_pck(1)), ("v2.pck", godot_pck(2)), ("v3.pck", godot_pck(3)), ("cr5.pak", chromium_pak(5)), ("cr4.pak", chromium_pak(4)), ("ue3.pak", unreal_pak(3)), ("ue7.pak", unreal_pak(7)), ("ue8.pak", unreal_pak(8, 4)), ("ue9.pak", unreal_pak(9)), ("ue11.pak", unreal_pak(11)), ("test.sfl", sfl()), ("bw2.stuff", bw2stuff()), ("nf1.007", nightfire(1)), ("nf3.007", nightfire(3)), ("aow.dat", edat()), ("test.m4b", ubibf()), ("test.adf", adf()), ("ufs7.bundle", unityfs(7, 3, 3)), ("ufs6.unity3d", unityfs(6, 1, 1)), ("ufs8.bundle", unityfs(8, 0, 0, True)), ("ufs8b.bundle", unityfs(8, 2, 2)),
                   ("ufs22.bundle", unityfs(7, 3, 3, files=unity_harness_bundle())),
                   ("empty8.bundle", unityfs(8, 0, 0, files=[])), ("empty6.unity3d", unityfs(6, 1, 1, files=[])),
                   ("obj17.bundle", unityfs(7, 3, 3, files=unity_objects_fixture(17))), ("obj19.bundle", unityfs(7, 3, 3, files=unity_objects_fixture(19))), ("obj21.bundle", unityfs(7, 3, 3, files=unity_objects_fixture(21, True))), ("obj22.bundle", unityfs(7, 2, 2, files=unity_objects_fixture(22))), ("obj22c.bundle", unityfs(6, 1, 1, files=unity_objects_fixture(22, True)))] + [
                   ("notree%d.bundle" % i, unityfs(7, 2, 2, files=unity_notree_fixture(i))) for i in range(4)] + [("texfmts.bundle", unityfs(7, 2, 2, files=unity_texfmts_fixture())), ("objs3.bundle", unityfs(7, 2, 2, files=unity_objs3_fixture(True))), ("objs3_notree.bundle", unityfs(7, 2, 2, files=unity_objs3_fixture(False)))] + unity_assets_fixtures() + xnb_fixtures() + __import__("rom_fixtures").all_fixtures() + [("test.lfd", lfd_fixture()), ("test.utx", ut_package_fixture()), ("flor.pak", florensia_fixture()), ("ac.forge", forge_fixture()), ("heath.nrm", nrm_fixture()), ("test.xwc", mos_xwc_fixture()), ("test.xtc", mos_xtc_fixture()), ("cnc.mix", cnc_mix_fixture()), ("ww.pak", wwpak_fixture()), ("test.asr", asura_fixtures()[0]), ("testz.asr", asura_fixtures()[1]), ("test.mrc", mrc_fixture()), ("test.lug", lionhead_fixture()), ("nox.bag", bagidx_fixtures()[0]), ("nox.idx", bagidx_fixtures()[1]), ("rom2.res", rom2res_fixture()), ("c3.pck", pck3_fixture()), ("test.h4r", h4r_fixture()), ("d3.hog", hog2_fixture()), ("mr2.bkf", bkf2_fixture()), ("test.jam", jam_fixture()), ("test.gjd", gjd7_fixtures()[0]), ("test.rl", gjd7_fixtures()[1]), ("test.dni", dni_fixture()), ("f22.dat", f22_fixture()), ("lego.dat", lego_fixture()), ("test.tex", gtex_fixture()), ("junk.zip", zip_fixture() + b"enuR" + bytes(18)), ("test.sdt", sdt_fixture()), ("test.gl", star_crusader_fixture()), ("sc.pak", star_crusader_fixture()), ("test.xcr", xcr_fixture()), ("test.cbf", cbf_fixture(False)), ("crypt.cbf", cbf_fixture(True)), ("test.wad", dawl_fixture()), ("test.dta", dta_cnt_fixtures()[0]), ("test.cnt", dta_cnt_fixtures()[1]), ("test.xwb", xwb_fixture()), ("test.win", gamemaker_fixture()), ("test.zip", zip_fixture()), ("sfx.zip", zip_fixture(b"MZ" + bytes(100))), ("test.fsb", fsb5(2, FSB_SAMPLES)), ("mpeg.fsb", fsb5(11, [(b"a", _mpeg_frames(3), 8, 2, None, 3 * 1152), (b"b", _mpeg_frames(2), 8, 2, None, 2 * 1152), (b"multi", _mpeg_frames(2, 16), 8, 6, None, 2304)], True)), ("vorbis.fsb", fsb5(15, VORBIS_SAMPLES)), ("vorbis.bank", fmod_bank(fsb5(15, VORBIS_SAMPLES))),
                   ("test.bank", fmod_bank(fsb5(15, FSB_SAMPLES))), ("empty.bank", b"RIFF" + struct.pack("<I", 12) + b"FEV " + b"FMT " + struct.pack("<II", 4, 0x40))]:
    with open(os.path.join(out, name), "wb") as f:
        f.write(blob)

drh, drd = dune_rfh()
open(os.path.join(out, "dune.rfh"), "wb").write(drh)
open(os.path.join(out, "dune.rfd"), "wb").write(drd)
gdir, gimg = gta3()
open(os.path.join(out, "gta3.dir"), "wb").write(gdir)
open(os.path.join(out, "gta3.img"), "wb").write(gimg)
# ---- Requiem: Avenging Angel: DCL (PKWARE implode) streams, texture archives (.DAT index + .ART data), levels (.JAM)
_DCL_LIT = [11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8, 9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5, 7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12, 8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27, 44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45, 44, 173]
_DCL_LEN = [2, 35, 36, 53, 38, 23]
_DCL_DIST = [2, 20, 53, 230, 247, 151, 248]
_DCL_BASE = [3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264]
_DCL_XTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8]


def _dcl_codes(rep):
    """symbol -> (code, length): canonical codes by length then symbol (the stream carries them inverted)"""
    lens = []
    for b in rep:
        lens += [b & 15] * ((b >> 4) + 1)
    codes, code = {}, 0
    for length in range(1, 17):
        for sym, l in enumerate(lens):
            if l == length:
                codes[sym] = (code, length)
                code += 1
        code <<= 1
    return codes


def dcl_implode(data, dict_bits=6):
    """binary mode DCL stream: literals and matches of 3..9 bytes (found by a naive search), end code"""
    ln, ds = _dcl_codes(_DCL_LEN), _dcl_codes(_DCL_DIST)
    bits = []

    def put(value, n):                      # LSB first
        bits.extend((value >> i) & 1 for i in range(n))

    def put_code(table, sym):               # Huffman codes go MSB first, inverted
        code, length = table[sym]
        bits.extend(1 - ((code >> (length - 1 - i)) & 1) for i in range(length))

    def put_len(length):
        sym = next(i for i in range(16) if _DCL_BASE[i] <= length < _DCL_BASE[i] + (1 << _DCL_XTRA[i]))
        put(1, 1)
        put_code(ln, sym)
        put(length - _DCL_BASE[sym], _DCL_XTRA[sym])

    i = 0
    while i < len(data):
        best = (0, 0)
        for d in range(1, min(i, 1 << (dict_bits + 6)) + 1):
            n = 0
            while n < 9 and i + n < len(data) and data[i + n - d] == data[i + n]:
                n += 1
            if n > best[0]:
                best = (n, d)
        if best[0] >= 3:
            n, d = best
            put_len(n)
            put_code(ds, (d - 1) >> dict_bits)
            put((d - 1) & ((1 << dict_bits) - 1), dict_bits)
            i += n
        else:
            put(0, 1)
            put(data[i], 8)
            i += 1
    put_len(519)
    out = bytearray([0, dict_bits])
    for k in range(0, len(bits), 8):
        out.append(sum(b << j for j, b in enumerate(bits[k:k + 8])))
    return bytes(out)


def req_texture(w, h, bpp, palette, pixels, levels=1, trailer=b""):
    head = struct.pack("<HH", w, h) + bytes(9) + bytes([levels, bpp, 2, 0, 0])
    return head + (palette or b"") + pixels + trailer


def req_art(textures, empty_slots=(1,)):
    """(dat, art): record = unpacked size, 0, packed size (counts itself), DCL stream; slots out of file order"""
    art, offsets = b"", []
    for t in textures:
        offsets.append(len(art))
        packed = dcl_implode(t)
        art += struct.pack("<III", len(t), 0, len(packed) + 4) + packed
    slots = [0xFFFFFFFF] * (len(offsets) + len(empty_slots))
    free = [i for i in range(len(slots)) if i not in empty_slots]
    for i, off in zip(free, offsets[:1] + offsets[1:][::-1]):    # the slots are not in the order of the file
        slots[i] = off
    return b"".join(struct.pack("<I", v) for v in slots), art


def req_art_fixtures():
    pal = b"".join(bytes([i, 255 - i, i // 2, 255]) for i in range(256))        # BGRA
    px8 = bytes((x * 7 + y * 3) & 255 for y in range(4) for x in range(4))
    px16 = b"".join(bytes([(x + y * 4) & 255, 0 if x < 2 else 255]) for y in range(4) for x in range(4))
    mips = bytes((i * 5) & 255 for i in range(16 + 4 + 1))
    d3d = [req_texture(4, 4, 1, pal, px8), req_texture(4, 4, 2, pal, px16), req_texture(0, 0, 1, None, b"junk")]
    sw = [req_texture(4, 4, 1, None, mips, levels=3), req_texture(4, 4, 2, None, px16, trailer=bytes(112))]
    return req_art(d3d), req_art(sw)


def req_jam(version, second=True):
    level = bytes((i * i // 7) & 255 for i in range(3000)) + b"\0" * 500 + b"Requiem" * 100
    packed = dcl_implode(level)
    rest = bytes(range(200))
    head = struct.pack("<IIIIII", 0xFEED000A, 0, len(level), 7, version, 250)
    head += bytes(0x112 + version * 0x100C - len(head))
    body = head + struct.pack("<I", len(packed)) + packed + rest
    if second:      # the level data block: magic at 4, two palettes (RGBX) at 0x1F and 0x41F
        data = bytearray(0x900)
        struct.pack_into("<I", data, 4, 0x0BEAF0CC)
        for i in range(256):
            data[0x41F + i * 4:0x41F + i * 4 + 4] = bytes([i, 255 - i, i // 2, 0])
        blob = dcl_implode(bytes(data))
        body = body[:4] + struct.pack("<I", len(body) - 12) + body[8:] + struct.pack("<I", len(data)) + blob
    return body


def req_mdl_face(indices, uvs):
    return struct.pack("<I", len(indices)) + bytes(16) + b"".join(struct.pack("<H", i) for i in indices) + b"".join(
        struct.pack("<ii", int(u * 65536), int(v * 65536)) for u, v in uvs)


def req_mdl_static():
    """a quad and a triangle, flags 1: one normal per face"""
    verts = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0), (0.5, 2, 0)]
    body = b"".join(struct.pack("<3f", *v) for v in verts)
    body += req_mdl_face([0, 1, 2, 3], [(0.5, 127.5), (127.5, 127.5), (127.5, 0.5), (0.5, 0.5)])
    body += req_mdl_face([3, 2, 4], [(0.5, 0.5), (127.5, 0.5), (64, 0.5)])
    body += struct.pack("<3f", 0, 0, 1) * 2
    return bytes([1]) + struct.pack("<I", 9) + b"test.bmp\0" + struct.pack("<II", len(verts), 2) + body


def req_mdl_skeletal():
    """two limbs of 3 and 4 vertices, one triangle each, flags 0x10: one normal per vertex"""
    name = b"skin.bmp\0"
    head = struct.pack("<IIIIB3xHH", 2, 0, 2, 0, 0x10, 7, len(name)) + bytes(0x2C - 0x18)
    sections = b""
    for code, count in ((0xFFFF0003, 3), (0xFFFF0000, 4)):     # limb 0 has one child, limb 1 is a leaf
        sections += struct.pack("<I", code) + bytes(0x1C) + struct.pack("<I", count) + bytes(0x38 - 0x24)
    verts = b"".join(struct.pack("<3f", i, i * 2, 0) for i in range(7))
    faces = req_mdl_face([0, 1, 2], [(0.5, 0.5), (1.5, 0.5), (0.5, 1.5)]) + req_mdl_face([3, 4, 5], [(0.5, 0.5), (1.5, 0.5), (0.5, 1.5)])
    return head + name + sections + verts + faces + struct.pack("<3f", 0, 1, 0) * 7


def req_mdl_animation():
    """body for req_skel.mdl: limb 0 turned 90 degrees around z at (10, 0, 0), limb 1 at (1, 0, 0) in its frame"""
    body = struct.pack("<I", 0xAA)                                       # rotation and position, constant, 2 limbs
    body += struct.pack("<4f3f", math.sqrt(0.5), 0, 0, math.sqrt(0.5), 10, 0, 0)
    body += struct.pack("<4f3f", 1, 0, 0, 0, 1, 0, 0)
    return struct.pack("<IIII", 0, 2, 0, len(body)) + bytes(0x34 - 16) + body


def req_anim():
    """(ary, bin): records of 16 bytes (offset, 0, size, 0xCDCDCDCD) that fill the .bin"""
    parts = [b"first record", bytes(range(50)), b"x"]
    ary, off = struct.pack("<I", len(parts)), 0
    for p in parts:
        ary += struct.pack("<IIII", off, 0, len(p), 0xCDCDCDCD)
        off += len(p)
    return ary, b"".join(parts)


(rd, ra), (rsd, rsa) = req_art_fixtures()
open(os.path.join(out, "req_d3d.dat"), "wb").write(rd)
open(os.path.join(out, "req_d3d.art"), "wb").write(ra)
open(os.path.join(out, "req_sw.dat"), "wb").write(rsd)
open(os.path.join(out, "req_sw.art"), "wb").write(rsa)
open(os.path.join(out, "req_static.mdl"), "wb").write(req_mdl_static())
open(os.path.join(out, "req_skel.mdl"), "wb").write(req_mdl_skeletal())
open(os.path.join(out, "req_skel.ani"), "wb").write(req_mdl_animation())
_ary, _bin = req_anim()
open(os.path.join(out, "req_anim.ary"), "wb").write(_ary)
open(os.path.join(out, "req_anim.bin"), "wb").write(_bin)
for v in (0, 1, 2):
    open(os.path.join(out, "req_v%d.jam" % v), "wb").write(req_jam(v, v != 1))

here = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(out, "payload.bin"), "wb") as f:
    f.write(RNC_PAYLOAD)
for m in (1, 2):
    blob = open(os.path.join(here, "fixtures", "payload_m%d.rnc" % m), "rb").read()
    with open(os.path.join(out, "payload_m%d.rnc" % m), "wb") as f:
        f.write(blob)
    if m == 1:      # damaged packed data: the CRC must catch it
        bad = bytearray(blob)
        bad[100] ^= 0x55
        with open(os.path.join(out, "corrupt.rnc"), "wb") as f:
            f.write(bad)
