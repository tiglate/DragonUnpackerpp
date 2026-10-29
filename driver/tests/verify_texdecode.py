#!/usr/bin/env python3
"""Checks the texture decoders of the driver against texture2ddecoder (pip install texture2ddecoder, or give
the folder in TEXTURE2DDECODER_PATH): ETC1/ETC2/EAC and ASTC on random blocks, the crunch decoder on files made by
crunch_enc.py. Usage: verify_texdecode.py texdec_dump crn_dump   (verify_texdecode.sh builds the tools and runs this)
The ASTC blocks that the decoder finds invalid are not compared (the reference reads out of its buffers on them)."""
import collections, os, random, struct, subprocess, sys, tempfile
if os.environ.get("TEXTURE2DDECODER_PATH"):
    sys.path.insert(0, os.environ["TEXTURE2DDECODER_PATH"])
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import texture2ddecoder as t
except ImportError:
    print("texture2ddecoder is not installed: skipped")
    sys.exit(0)
import crunch_enc as E
tool, crn = sys.argv[1], sys.argv[2]
failed = 0


def etc():
    global failed
    random.seed(7)
    F = {34: (t.decode_etc1, 8), 45: (t.decode_etc2, 8), 46: (t.decode_etc2a1, 8), 47: (t.decode_etc2a8, 16),
         10: (t.decode_bc1, 8), 12: (t.decode_bc3, 16), 41: (t.decode_eacr, 8), 42: (t.decode_eacr_signed, 8), 43: (t.decode_eacrg, 16), 44: (t.decode_eacrg_signed, 16)}
    n = 0
    for fmt, (fn, bb) in F.items():
        for w, h in ((4, 4), (16, 8), (37, 21), (64, 64), (1, 1), (5, 3), (512, 512), (333, 205)):
            nb = ((w + 3) // 4) * ((h + 3) // 4)
            data = random.randbytes(nb * bb)
            with tempfile.TemporaryDirectory() as d:
                open(d + "/in", "wb").write(data)
                subprocess.run([tool, str(fmt), str(w), str(h), d + "/in", d + "/out"], check=True, capture_output=True)
                mine = open(d + "/out", "rb").read()
            n += 1
            if mine != fn(data, w, h):
                print("ETC/EAC differs: format", fmt, w, h); failed += 1
    print("etc/eac: %d images compared" % n)


def astc():
    global failed
    random.seed(11)
    FM = {48: 4, 49: 5, 50: 6, 51: 8, 52: 10, 53: 12, 54: 4, 55: 5, 56: 6, 57: 8, 58: 10, 59: 12, 66: 4, 67: 5, 68: 6, 69: 8, 70: 10, 71: 12}
    same = invalid = 0
    for fmt, bs in FM.items():
        for w, h in ((bs * 24, bs * 20), (bs * 7 + 3, bs * 5 + 1)):
            nx, ny = (w + bs - 1) // bs, (h + bs - 1) // bs
            blocks = bytearray()
            for _ in range(nx * ny):
                b = bytearray(random.randbytes(16))
                r = random.random()
                if r < 0.08:
                    b[0] = 0xFC; b[1] = (b[1] & 0xFC) | 1 | random.choice((0, 2))
                elif r < 0.4:
                    b[0] = (b[0] & 0x03) | 0x40 | (b[0] & 0x20); b[1] &= 0xE7
                blocks += b
            with tempfile.TemporaryDirectory() as d:
                open(d + "/in", "wb").write(bytes(blocks))
                subprocess.run([tool, str(fmt), str(w), str(h), d + "/in", d + "/out", d + "/ok"], check=True, capture_output=True)
                mine = open(d + "/out", "rb").read(); ok = open(d + "/ok", "rb").read()
            safe = bytearray(blocks)
            for i in range(nx * ny):
                if not ok[i]:
                    safe[i * 16:(i + 1) * 16] = bytes([0xFC, 0xFD]) + bytes(14)
            ref = t.decode_astc(bytes(safe), w, h, bs, bs)
            for by in range(ny):
                for bx in range(nx):
                    if not ok[by * nx + bx]:
                        invalid += 1; continue
                    good = True
                    for y in range(by * bs, min(h, by * bs + bs)):
                        a = (y * w + bx * bs) * 4; z = (y * w + min(w, bx * bs + bs)) * 4
                        if mine[a:z] != ref[a:z]: good = False; break
                    if good: same += 1
                    else:
                        failed += 1; print("ASTC block differs: format", fmt, bx, by)
    print("astc: %d valid blocks compared, %d invalid ones skipped" % (same, invalid))


def crunch():
    global failed
    random.seed(5)
    n = 0
    for fmt in (0, 2):
        for w, h in ((8, 8), (16, 12), (4, 4), (20, 8), (36, 28), (1, 1)):
            bx, by = (w + 3) // 4, (h + 3) // 4
            pc = [random.getrandbits(32) for _ in range(5)]; ps = [random.getrandbits(32) for _ in range(6)]
            pa = [random.getrandbits(16) for _ in range(3)]; pas = [random.getrandbits(48) for _ in range(4)]
            blocks = [(random.choice(pc), random.choice(ps)) if fmt == 0 else
                      (random.choice(pa), random.choice(pas), random.choice(pc), random.choice(ps)) for _ in range(bx * by)]
            data = E.encode(fmt, w, h, blocks)
            expect = b"".join(struct.pack("<II", *b) if fmt == 0 else
                              struct.pack("<HHHH", b[0], b[1] & 0xFFFF, (b[1] >> 16) & 0xFFFF, (b[1] >> 32) & 0xFFFF) + struct.pack("<II", b[2], b[3])
                              for b in blocks)
            with tempfile.TemporaryDirectory() as d:
                open(d + "/c", "wb").write(data)
                r = subprocess.run([crn, d + "/c", d + "/o"], capture_output=True)
                mine = open(d + "/o", "rb").read() if r.returncode == 0 else None
            n += 1
            if mine != expect or t.unpack_unity_crunch(data) != expect:
                failed += 1; print("crunch differs: format", fmt, w, h, mine == expect)
    print("crunch: %d files compared with the reference unpacker" % n)


etc()
astc()
crunch()
print("texture decoders: " + ("%d PROBLEM(S)" % failed if failed else "ALL OK"))
sys.exit(1 if failed else 0)
