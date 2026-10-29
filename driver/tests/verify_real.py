#!/usr/bin/env python3
"""Independent reader (plain python) of the real files to compare with the
driver output. Usage: verify_real.py archive cli_output"""
import hashlib, os, struct, sys

path, outp = sys.argv[1], sys.argv[2]
f = open(path, "rb")
head = f.read(16)
ents = []   # name, offset, size
chromium = False
ident = head[:4]
if ident == b"PACK":
    off, size = struct.unpack("<II", head[4:12])
    f.seek(off)
    for _ in range(size // 64):
        n, o, s = struct.unpack("<56sII", f.read(64))
        ents.append((n.split(b"\0")[0].decode("latin-1"), o, s))
elif ident in (b"IWAD", b"PWAD"):
    num, off = struct.unpack("<II", head[4:12])
    f.seek(off)
    for _ in range(num):
        o, s, n = struct.unpack("<II8s", f.read(16))
        ents.append((n.split(b"\0")[0].decode("latin-1"), o, s))
elif ident == b"GOB\n":
    off = struct.unpack("<I", head[4:8])[0]
    f.seek(off)
    num = struct.unpack("<I", f.read(4))[0]
    for _ in range(num):
        o, s, n = struct.unpack("<II13s", f.read(21))
        ents.append((n.split(b"\0")[0].decode("latin-1"), o, s))
elif struct.unpack("<I", head[:4])[0] == 1 and path.lower().endswith(".art"):
    _, num, first, last = struct.unpack("<IIII", head)
    n = last - first + 1
    f.seek(16)
    ws = struct.unpack("<%dH" % n, f.read(2 * n)); hs = struct.unpack("<%dH" % n, f.read(2 * n))
    o = 16 + 8 * n
    for i in range(n):
        if ws[i] * hs[i]:
            ents.append(("texture #%06d.tex" % (i + first), o, ws[i] * hs[i])); o += ws[i] * hs[i]
elif head[:12] == b"KenSilverman":
    num = struct.unpack("<I", head[12:16])[0]
    f.seek(16)
    o = 16 + 16 * num
    for _ in range(num):
        n, sz = struct.unpack("<12sI", f.read(16))
        ents.append((n.split(b"\0")[0].decode("latin-1"), o, sz))
        o += sz
elif ident[:3] == b"DHF":
    total = os.path.getsize(path)
    pos = 3
    while pos < total:
        f.seek(pos)
        n, sz = struct.unpack("<13sI", f.read(17))
        ents.append((n.split(b"\0")[0].decode("latin-1"), pos + 17, sz))
        pos += 17 + sz
elif ident == b"RFF\x1a":
    ver, off, num = struct.unpack("<III", head[4:16])
    f.seek(off)
    raw = bytearray(f.read(num * 48))
    if ver >= 0x301:
        for i in range(len(raw)):
            raw[i] ^= ((2 * off + i) >> 1) & 0xFF
    for i in range(num):
        _, o, s, _, _, fl, ext, n, _ = struct.unpack("<16sIIIIB3s8sI", bytes(raw[i * 48:i * 48 + 48]))
        n = n.split(b"\0")[0].decode("latin-1"); ext = ext.split(b"\0")[0].decode("latin-1")
        ents.append((n + ("." + ext if ext else ""), o, s, fl & 0x10))
elif head[:7] == b"UnityFS":
    # independent reader (unity_ref.py): the files of the bundle and the objects of its serialized files
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import unity_ref
    exp = unity_ref.expected(path)
    got = [l.rstrip("\n").split("\t") for l in open(outp, encoding="latin-1") if not l.startswith("#")]
    bad = 0
    if len(got) != len(exp):
        print("COUNT MISMATCH", len(got), len(exp)); bad += 1
    for (idx, off, size, md5, name), (en, es, emd5, _) in zip(got, exp):
        if name != en or int(size) != es or (emd5 is not None and md5 != emd5):
            print("MISMATCH", idx, name, en, size, es); bad += 1
            if bad > 10: break
    print("%s: %d entries, %s" % (path.split("/")[-1], len(exp), "OK" if not bad else "%d PROBLEM(S)" % bad))
    sys.exit(1 if bad else 0)
elif ident == b"FSB5" or (ident == b"RIFF" and head[8:12] == b"FEV "):
    # FMOD sound bank: the sounds as the driver converts them (see fsb_ref.py)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import fsb_ref
    exp = fsb_ref.expected(path)
    got = [l.rstrip("\n").split("\t") for l in open(outp, encoding="latin-1") if not l.startswith("#")]
    bad = 0
    if len(got) != len(exp):
        print("COUNT MISMATCH", len(got), len(exp)); bad += 1
    for (idx, off, size, md5, name), (en, eo, es, edata) in zip(got, exp):
        if name != en or (int(off), int(size)) != (eo, es) or md5 != hashlib.md5(edata).hexdigest():
            print("MISMATCH", idx, name, en, size, es); bad += 1
            if bad > 10: break
    print("%s: %d entries, %s" % (path.split("/")[-1], len(exp), "OK" if not bad else "%d PROBLEM(S)" % bad))
    sys.exit(1 if bad else 0)
elif head[:3] == b"XNB":
    # XNA content file: one entry (converted texture / sound / effect or the unpacked content)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    buf = open(path, "rb").read()
    flags, fsz = buf[5], struct.unpack("<I", buf[6:10])[0]
    got = [l.rstrip("\n").split("\t") for l in open(outp, encoding="latin-1") if not l.startswith("#")]
    if flags & 0x80:
        print("%s: LZX compressed, not checked here (%d entry)" % (path.split("/")[-1], len(got)))
        sys.exit(0 if len(got) == 1 else 1)
    if flags & 0x40:
        import unity_ref
        n, = struct.unpack("<I", buf[10:14])
        c = unity_ref.lz4_block(buf[14:fsz], n)
    else:
        c = buf[10:]          # (the size in the header is wrong in some files)
    p = [0]
    def r7():
        v = s = 0
        while True:
            b = c[p[0]]; p[0] += 1; v |= (b & 127) << s; s += 7
            if not b & 128: return v
            if s > 28: raise ValueError
    nr = r7(); readers = []
    for _ in range(nr):
        ln = r7(); readers.append(c[p[0]:p[0] + ln].decode("latin-1").split(",")[0]); p[0] += ln + 4
    r7(); tid = r7()
    nm = os.path.basename(path)
    nm = nm[:-4] if nm.lower().endswith(".xnb") else nm
    nm = os.path.basename(outp) and nm
    rd = readers[tid - 1] if 1 <= tid <= nr else ""
    data = None
    if rd.endswith("SpriteFontReader") and buf[4] == 5:
        # the atlas (as a texture entry) and a BMFont text
        def i32():
            v, = struct.unpack("<i", c[p[0]:p[0] + 4]); p[0] += 4; return v
        r7()
        fm, w, h, mips, sz = struct.unpack("<5I", c[p[0]:p[0] + 20]); tex = (fm, w, h, c[p[0] + 20:p[0] + 20 + sz])
        p[0] += 16
        for _ in range(mips):
            q, = struct.unpack("<I", c[p[0]:p[0] + 4]); p[0] += 4 + q
        r7(); k = i32(); gl = [struct.unpack("<4i", c[p[0] + 16 * i:p[0] + 16 * i + 16]) for i in range(k)]; p[0] += 16 * k
        r7(); k = i32(); cr = [struct.unpack("<4i", c[p[0] + 16 * i:p[0] + 16 * i + 16]) for i in range(k)]; p[0] += 16 * k
        r7(); k = i32(); chars = []
        for _ in range(k):
            b = c[p[0]]; ln = 1 if b < 0x80 else 2 if b < 0xE0 else 3
            chars.append(ord(c[p[0]:p[0] + ln].decode("utf-8", "replace"))); p[0] += ln
        line = i32(); spacing, = struct.unpack("<f", c[p[0]:p[0] + 4]); p[0] += 4
        r7(); k = i32(); kern = [struct.unpack("<3f", c[p[0] + 12 * i:p[0] + 12 * i + 12]) for i in range(k)]
        fm, w, h, px = tex
        if fm in (4, 5, 6):
            hd = b"DDS " + struct.pack("<7I", 124, 0x81007, h, w, len(px), 0, 0) + bytes(44) + struct.pack("<II4s", 32, 4, {4: b"DXT1", 5: b"DXT3", 6: b"DXT5"}[fm]) + bytes(20) + struct.pack("<I", 0x1000) + bytes(16)
            atlas, ext = hd + px, ".dds"
        else:
            out = bytearray()
            for i in range(0, len(px), 4): out += bytes((px[i + 2], px[i + 1], px[i], px[i + 3]))
            atlas, ext = bytes((0, 0, 2)) + bytes(9) + struct.pack("<HH", w, h) + bytes((32, 0x28)) + bytes(out), ".tga"
        txt = ('info face="%s" size=%d bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=0 aa=1 padding=0,0,0,0 spacing=0,0\n'
               'common lineHeight=%d base=%d scaleW=%d scaleH=%d pages=1 packed=0\npage id=0 file="%s%s"\nchars count=%d\n' % (nm, line, line, line, w, h, nm, ext, len(gl)))
        rnd = lambda f: int(round(f))      # (python rounds halves to even, the driver away from zero: values are whole numbers or x.5 only by chance)
        for i in range(len(gl)):
            a, b, cc = kern[i]
            txt += "char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d page=0 chnl=15\n" % (
                chars[i], gl[i][0], gl[i][1], gl[i][2], gl[i][3], cr[i][0] + rnd(a), cr[i][1], rnd(a + b + cc + spacing))
        exp2 = [(nm + ext, atlas), (nm + ".fnt", txt.encode())]
        ok = len(got) == 2 and all(g[4] == e[0] and int(g[2]) == len(e[1]) and g[3] == hashlib.md5(e[1]).hexdigest() for g, e in zip(got, exp2))
        print("%s: 2 entries, %s" % (path.split("/")[-1], "OK" if ok else "PROBLEM %s" % [(g[4], g[2]) for g in got]))
        sys.exit(0 if ok else 1)
    if rd.endswith("Texture2DReader") and buf[4] == 5:
        fm, w, h, mips, sz = struct.unpack("<5I", c[p[0]:p[0] + 20]); px = c[p[0] + 20:p[0] + 20 + sz]
        if fm in (4, 5, 6):
            hd = b"DDS " + struct.pack("<7I", 124, 0x81007, h, w, sz, 0, 0) + bytes(44) + struct.pack("<II4s", 32, 4, {4: b"DXT1", 5: b"DXT3", 6: b"DXT5"}[fm]) + bytes(20) + struct.pack("<I", 0x1000) + bytes(16)
            data, ext = hd + px, ".dds"
        elif fm in (0, 1, 2, 3, 12):
            out = bytearray()
            if fm == 0:
                for i in range(0, len(px), 4): out += bytes((px[i + 2], px[i + 1], px[i], px[i + 3]))
            elif fm == 12: out = bytearray(px)
            else:
                for i in range(0, len(px), 2):
                    v = px[i] | px[i + 1] << 8
                    e5 = lambda x: (x << 3) | (x >> 2)
                    if fm == 1: out += bytes((e5(v & 31), (((v >> 5) & 63) << 2) | (((v >> 5) & 63) >> 4), e5(v >> 11), 255))
                    elif fm == 2: out += bytes((e5(v & 31), e5((v >> 5) & 31), e5((v >> 10) & 31), 255 if v & 0x8000 else 0))
                    else: out += bytes((((v >> k) & 15) * 17 for k in (0, 4, 8, 12)))
            hd = bytes((0, 0, 3 if fm == 12 else 2)) + bytes(9) + struct.pack("<HH", w, h) + bytes((8 if fm == 12 else 32, 0x20 if fm == 12 else 0x28))
            data, ext = hd + bytes(out), ".tga"
    elif rd.endswith("SoundEffectReader"):
        fl, = struct.unpack("<I", c[p[0]:p[0] + 4]); fmt = c[p[0] + 4:p[0] + 4 + fl]; dl, = struct.unpack("<I", c[p[0] + 4 + fl:p[0] + 8 + fl]); pcm = c[p[0] + 8 + fl:p[0] + 8 + fl + dl]
        data, ext = b"RIFF" + struct.pack("<I", 4 + 8 + fl + 8 + dl) + b"WAVEfmt " + struct.pack("<I", fl) + fmt + b"data" + struct.pack("<I", dl) + pcm, ".wav"
    if data is None:
        short = rd.split(".")[-1].lower()
        short = short[:-6] if short.endswith("reader") else short
        short = "".join(ch if ch.isalnum() else "_" for ch in short) or "content"
        data, ext = c, "." + short + ".bin"
        if short == "effect":
            n, = struct.unpack("<I", c[p[0]:p[0] + 4]); data, ext = c[p[0] + 4:p[0] + 4 + n], ".fxb"
    ok = len(got) == 1 and got[0][4] == nm + ext and int(got[0][2]) == len(data) and got[0][3] == hashlib.md5(data).hexdigest()
    print("%s: 1 entries, %s" % (path.split("/")[-1], "OK" if ok else "PROBLEM %s vs %s%s %d" % (got, nm, ext, len(data))))
    sys.exit(0 if ok else 1)
elif ident == b"GDPC":
    # Godot pack format 0/1 (Godot 3): 84 bytes header, then the number of files
    ver = struct.unpack("<I", head[4:8])[0]
    if ver > 1:
        sys.exit("only Godot pack format 0/1 is checked here")
    f.seek(84)
    num = struct.unpack("<I", f.read(4))[0]
    for _ in range(num):
        ln = struct.unpack("<I", f.read(4))[0]
        n = f.read(ln).split(b"\0")[0].decode("utf-8")
        o, s, _md5 = struct.unpack("<QQ16s", f.read(32))
        ents.append((n[6:] if n.startswith("res://") else n, o, s))
elif head[:4] == b"\x05\x00\x00\x00" and path.lower().endswith(".pak"):
    # Chromium data pack version 5: resource ids only, the driver adds a guessed extension
    chromium = True
    enc, nres, nalias = struct.unpack("<B3xHH", head[4:12])
    f.seek(12)
    recs = [struct.unpack("<HI", f.read(6)) for _ in range(nres + 1)]
    for i in range(nres):
        ents.append((str(recs[i][0]), recs[i][1], recs[i + 1][1] - recs[i][1]))
elif ident[:3] == b"RNC":
    # ground truth: the reference tool (RNC_TOOL=/path/to/rnc built from rnc_propack_source)
    import os, subprocess, tempfile
    tool = os.environ.get("RNC_TOOL")
    if not tool:
        sys.exit("set RNC_TOOL to the reference rnc binary to verify RNC files")
    with tempfile.TemporaryDirectory() as td:
        import shutil
        shutil.copy(path, td + "/in.rnc")
        subprocess.run([tool, "u", "in.rnc"], check=True, stdout=subprocess.DEVNULL, cwd=td)
        outs = [n for n in os.listdir(td) if n != "in.rnc"]    # the tool picks the output name
        want = hashlib.md5(open(td + "/" + outs[0], "rb").read()).hexdigest()
    got = [l.rstrip("\n").split("\t") for l in open(outp, encoding="latin-1") if not l.startswith("#")]
    ok = len(got) == 1 and got[0][3] == want
    print("%s: RNC, %s" % (path.split("/")[-1], "OK" if ok else "MISMATCH %s vs %s" % (got, want)))
    sys.exit(0 if ok else 1)
else:
    # stand-alone Unity serialized file (the .resS companions are read from its folder)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import unity_ref
    try:
        exp = unity_ref.expected_assets(path)
    except Exception:
        sys.exit("unknown format " + repr(ident))
    got = [l.rstrip("\n").split("\t") for l in open(outp, encoding="latin-1") if not l.startswith("#")]
    bad = 0
    if len(got) != len(exp):
        print("COUNT MISMATCH", len(got), len(exp)); bad += 1
    for (idx, off, size, md5, name), (en, es, emd5, _) in zip(got, exp):
        if name != en or int(size) != es or (emd5 is not None and md5 != emd5):
            print("MISMATCH", idx, name, en, size, es); bad += 1
            if bad > 10: break
    print("%s: %d entries, %s" % (path.split("/")[-1], len(exp), "OK" if not bad else "%d PROBLEM(S)" % bad))
    sys.exit(1 if bad else 0)

got = [l.rstrip("\n").split("\t") for l in open(outp, encoding="latin-1") if not l.startswith("#")]
bad = 0
if len(got) != len(ents):
    print("COUNT MISMATCH", len(got), len(ents)); bad += 1
for (idx, off, size, md5, name), ent in zip(got, ents):
    en, eo, es = ent[:3]
    if len(ent) > 3 and isinstance(ent[3], bytes):
        data = bytearray(ent[3])
    else:
        f.seek(eo)
        data = bytearray(f.read(es))
    if len(ent) > 3 and not isinstance(ent[3], bytes) and ent[3]:          # RFF: first 256 bytes are encrypted
        for i in range(min(256, len(data))):
            data[i] ^= (i >> 1) & 0xFF
    if chromium:
        name_ok = name.split(".")[0] == en
    else:
        name_ok = name == en
    if not name_ok or (int(off), int(size)) != (eo, es) or md5 != hashlib.md5(bytes(data)).hexdigest():
        print("MISMATCH", idx, name, en); bad += 1
        if bad > 10: break
print("%s: %d entries, %s" % (path.split("/")[-1], len(ents), "OK" if not bad else "%d PROBLEM(S)" % bad))
sys.exit(1 if bad else 0)
