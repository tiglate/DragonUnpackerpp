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


def all_fixtures():
    return [("test.nes", nes()[0]), ("test.sfc", snes()[0]), ("copier.smc", snes(True)[0]), ("test.md", megadrive()[0]), ("test.smd", smd()[0])]
