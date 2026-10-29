"""Independent reference (plain python) for the Unity asset bundles: the files of a UnityFS bundle
and the objects (textures, texts, sounds, fonts) of its serialized files as the driver lists them.
Used by verify_real.py. Usage: unity_ref.py bundle  (prints index name size md5)"""
import collections, hashlib, os, struct, sys
import lzma

SKIPS = collections.Counter()      # why serialized files / objects were left out (see unity_stats.py)

def lz4_block(src, usize):
    out = bytearray(); i = 0
    while i < len(src):
        t = src[i]; i += 1
        ll = t >> 4
        if ll == 15:
            while True:
                b = src[i]; i += 1; ll += b
                if b != 255: break
        out += src[i:i + ll]; i += ll
        if i >= len(src): break
        off = src[i] | (src[i + 1] << 8); i += 2
        ml = t & 15
        if ml == 15:
            while True:
                b = src[i]; i += 1; ml += b
                if b != 255: break
        ml += 4
        for _ in range(ml): out.append(out[-off])
    return bytes(out)
def unpack(m, d, u):
    if m == 0: return d
    if m == 1:
        dd = lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=[{"id": lzma.FILTER_LZMA1, "dict_size": 1 << 26, **lzma._decode_filter_properties(lzma.FILTER_LZMA1, d[:5])}])
        return dd.decompress(d[5:], u)
    return lz4_block(d, u)
ENGINE = [""]


def load(path):
    f = open(path, "rb"); raw = f.read(512)
    z = raw.index(b"\0", 12); z2 = raw.index(b"\0", z + 1)
    ENGINE[0] = raw[z + 1:z2].decode("latin-1")
    ver = struct.unpack(">I", raw[8:12])[0]
    size, cs, us, flags = struct.unpack(">qIII", raw[z2 + 1:z2 + 21])
    pos = z2 + 1 + 20
    if ver >= 7 or flags & 0x200: pos = (pos + 15) & ~15
    if flags & 0x80:
        f.seek(os.path.getsize(path) - cs); info = f.read(cs); dp = pos
    else:
        f.seek(pos); info = f.read(cs); dp = pos + cs
    info = unpack(flags & 0x3F, info, us)
    nb = struct.unpack(">I", info[16:20])[0]; p = 20; blocks = []
    for _ in range(nb):
        u, c, fl = struct.unpack(">IIH", info[p:p + 10]); p += 10; blocks.append((u, c, fl))
    nf = struct.unpack(">I", info[p:p + 4])[0]; p += 4
    cands = [(dp + 15) & ~15, dp] if (ver >= 7 or flags & 0x200) else [dp]
    for d in cands:
        try:
            f.seek(d); unpack(blocks[0][2] & 0x3F, f.read(blocks[0][1]), blocks[0][0]); dp = d; break
        except Exception: pass
    f.seek(dp); stream = b"".join(unpack(fl & 0x3F, f.read(c), u) for u, c, fl in blocks)
    files = []
    for _ in range(nf):
        o, sz, fl = struct.unpack(">qqI", info[p:p + 20]); p += 20
        e = info.index(b"\0", p); n = info[p:e].decode(); p = e + 1
        files.append((n, fl, stream[o:o + sz]))
    return files

COMMON = ("AABB\0AnimationClip\0AnimationCurve\0AnimationState\0Array\0Base\0BitField\0bitset\0bool\0char\0ColorRGBA\0Component\0data\0deque\0double\0dynamic_array\0FastPropertyName\0first\0float\0Font\0GameObject\0Generic Mono\0GradientNEW\0GUID\0GUIStyle\0int\0list\0long long\0map\0Matrix4x4f\0MdFour\0MonoBehaviour\0MonoScript\0m_ByteSize\0m_Curve\0m_EditorClassIdentifier\0m_EditorHideFlags\0m_Enabled\0m_ExtensionPtr\0m_GameObject\0m_Index\0m_IsArray\0m_IsStatic\0m_MetaFlag\0m_Name\0m_ObjectHideFlags\0m_PrefabInternal\0m_PrefabParentObject\0m_Script\0m_StaticEditorFlags\0m_Type\0m_Version\0Object\0pair\0PPtr<Component>\0PPtr<GameObject>\0PPtr<Material>\0PPtr<MonoBehaviour>\0PPtr<MonoScript>\0PPtr<Object>\0PPtr<Prefab>\0PPtr<Sprite>\0PPtr<TextAsset>\0PPtr<Texture>\0PPtr<Texture2D>\0PPtr<Transform>\0Prefab\0Quaternionf\0Rectf\0RectInt\0RectOffset\0second\0set\0short\0size\0SInt16\0SInt32\0SInt64\0SInt8\0staticvector\0string\0TextAsset\0TextMesh\0Texture\0Texture2D\0Transform\0TypelessData\0UInt16\0UInt32\0UInt64\0UInt8\0unsigned int\0unsigned long long\0unsigned short\0vector\0Vector2f\0Vector3f\0Vector4f\0m_ScriptingClassIdentifier\0Gradient\0Type*\0int2_storage\0int3_storage\0BoundsInt\0m_CorrespondingSourceObject\0m_PrefabInstance\0m_PrefabAsset\0FileSize\0Hash128\0RenderingLayerMask\0").encode()


def cs(b, o):
    if o & 0x80000000:
        o &= 0x7fffffff
        assert o == 0 or COMMON[o - 1] == 0, "bad common offset"
        return COMMON[o:COMMON.index(b"\0", o)].decode("latin-1")
    return b[o:b.index(b"\0", o)].decode("latin-1")


class Node:
    pass


_TEMPLATES = None
def templates(cls):
    global _TEMPLATES
    if _TEMPLATES is None:
        import json
        _TEMPLATES = []
        for t in json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "unity_trees.json"))):
            nodes = []
            for lvl, ty, nm, sz, fl in t["tree"]:
                n = Node(); n.level, n.type, n.name, n.size, n.flags = lvl, ty, nm, sz, fl
                nodes.append(n)
            _TEMPLATES.append((t["cls"], nodes))
    return [n for c, n in _TEMPLATES if c == cls]


def plausible(cls, t):
    if cls == 28:
        sd = t.get("m_StreamData") or {}
        im = t.get("image data")
        im = len(im) if isinstance(im, bytes) else 0
        return (1 <= t.get("m_Width", -1) <= 32768 and 1 <= t.get("m_Height", -1) <= 32768 and
                1 <= t.get("m_TextureFormat", -1) <= 64 and t.get("m_CompleteImageSize") == im + sd.get("size", 0))
    if cls == 89:
        return 1 <= t.get("m_Width", -1) <= 16384 and 1 <= t.get("m_Height", -1) <= 16384 and 1 <= t.get("m_TextureFormat", -1) <= 64 and t.get("m_CompleteImageSize", -1) > 0
    if cls == 213:
        r = t.get("m_Rect") or {}
        fid = ((t.get("m_RD") or {}).get("texture") or {}).get("m_FileID", -99999)
        return (0 <= r.get("width", -1) < 1000000 and 0 <= r.get("height", -1) < 1000000 and 0 < t.get("m_PixelsToUnits", -1) < 1000000
                and -1000 <= fid < 1000)
    if cls == 43:
        vc = (t.get("m_VertexData") or {}).get("m_VertexCount", -1)
        return 0 <= t.get("m_MeshCompression", -1) <= 4 and 0 <= t.get("m_IndexFormat", 0) <= 1 and 0 <= vc <= 50000000
    if cls == 83:
        return 0 <= (t.get("m_Resource") or {}).get("m_Size", -1) and 0 <= t.get("m_Channels", -1) <= 64
    return True


class Serialized:
    def __init__(self, d):
        self.d = d
        md, fs, ver, do = struct.unpack(">IIII", d[:16])
        if not 17 <= ver <= 22 or d[16]:
            raise ValueError("unsupported")
        p = 20
        if ver >= 22:
            fs, do = struct.unpack(">qq", d[p + 4:p + 20])
            p += 28
        if do > len(d) or do <= p:
            raise ValueError("bad header")
        self.ver, self.do = ver, do
        e = d.index(b"\0", p)
        self.unity = d[p:e].decode("latin-1")
        p = e + 1 + 4
        self.notree = not d[p]
        p += 1
        nt, = struct.unpack("<i", d[p:p + 4])
        p += 4
        self.types = []
        for _ in range(nt):
            cid, = struct.unpack("<i", d[p:p + 4])
            p += 4 + 1 + 2
            p += 16 if cid == 114 else 0
            p += 16
            if self.notree:
                self.types.append((cid, None))
                continue
            nn, sl = struct.unpack("<II", d[p:p + 8])
            p += 8
            nsz = 32 if ver >= 19 else 24
            raw = d[p:p + nn * nsz]
            p += nn * nsz
            sb = d[p:p + sl]
            p += sl
            tree = []
            for i in range(nn):
                r = raw[i * nsz:(i + 1) * nsz]
                v, lvl, tf, to, no, bs, idx, mf = struct.unpack("<HBBIIiii", r[:24])
                n = Node()
                n.level, n.type, n.name, n.size, n.flags = lvl, cs(sb, to), cs(sb, no), bs, mf
                tree.append(n)
            if ver >= 21:
                nd, = struct.unpack("<i", d[p:p + 4])
                p += 4 + 4 * nd
            self.types.append((cid, tree))
        no, = struct.unpack("<i", d[p:p + 4])
        p += 4
        self.objs = []
        for _ in range(no):
            p = (p + 3) & ~3
            pid, = struct.unpack("<q", d[p:p + 8])
            p += 8
            if ver >= 22:
                bs, = struct.unpack("<q", d[p:p + 8])
                p += 8
            else:
                bs, = struct.unpack("<I", d[p:p + 4])
                p += 4
            sz, ti = struct.unpack("<Ii", d[p:p + 8])
            p += 8
            if bs + do + sz > len(d):
                raise ValueError("object outside")
            self.objs.append((pid, bs + do, sz, self.types[ti][0], ti))

    def read(self, o):
        """fields of an object: dict of name -> int | bytes | dict (arrays of structs are skipped)"""
        tree = self.types[o[4]][1]
        if tree is None:
            # no type tree: the built-in layout that reads exactly the object and gives sensible values
            for t in templates(o[3]):
                try:
                    v = self.read_tree(o, t, strict=True)
                except Exception:
                    continue
                if plausible(o[3], v):
                    return v
            raise ValueError("no layout fits")
        return self.read_tree(o, tree)

    def read_tree(self, o, tree, strict=False):
        d = self.d
        start = o[1]
        end = start + o[2]
        pos = [start]
        PRIM = {1: "B", 2: "H", 4: "I", 8: "Q"}

        def take(n):
            if pos[0] + n > end:
                raise ValueError("outside")
            v = d[pos[0]:pos[0] + n]
            pos[0] += n
            return v

        def subtree_end(i):
            j = i + 1
            while j < len(tree) and tree[j].level > tree[i].level:
                j += 1
            return j

        def walk(i):
            n = tree[i]
            j = subtree_end(i)
            val = None
            if n.type in ("Array", "TypelessData") and j > i + 2 and subtree_end(i + 1) == i + 2:
                ln, = struct.unpack("<i", take(4))
                if ln < 0:
                    raise ValueError("negative size")
                el = tree[i + 2]
                if subtree_end(i + 2) == i + 3 and el.size > 0:
                    raw = take(ln * el.size)
                    val = raw if el.size == 1 else None
                else:
                    val = [walk(i + 2) for _ in range(ln)]
            elif j == i + 1:
                if n.type == "float" and n.size == 4:
                    v, = struct.unpack("<f", take(4))
                elif n.type in ("SInt64", "SInt32", "SInt16", "SInt8", "int", "short", "long long"):
                    v, = struct.unpack("<" + {1: "b", 2: "h", 4: "i", 8: "q"}[n.size], take(n.size))
                else:
                    v, = struct.unpack("<" + PRIM[n.size], take(n.size))
                val = v
            else:
                val = {}
                k = i + 1
                while k < j:
                    val[tree[k].name] = walk(k)
                    k = subtree_end(k)
                if n.type in ("string", "vector") and list(val) == ["Array"]:   # a struct holding one Array
                    val = val["Array"]
            if n.flags & 0x4000:
                pos[0] = start + ((pos[0] - start + 3) & ~3)
            return val

        v = walk(0)
        if strict and pos[0] != end:
            raise ValueError("object not consumed")
        return v


def _sniff_audio(h):
    if h[:4] in (b"FSB5", b"FSB4"):
        return ".fsb"
    if h[:4] == b"RIFF":
        return ".wav"
    if h[:4] == b"OggS":
        return ".ogg"
    if h[:3] == b"ID3":
        return ".mp3"
    if len(h) >= 2 and h[0] == 0xFF and h[1] & 0xE0 == 0xE0:
        return ".mp3"
    return ".bin"


def _sniff_text(h, ln):
    s = 3 if h[:3] == b"\xef\xbb\xbf" else 0
    if any(c < 32 and c not in (9, 10, 13) for c in h[s:]):
        return ".bytes"
    if s < len(h) and ln > 0:
        if h[s:s + 1] in (b"{", b"["):
            return ".json"
        if h[s:s + 1] == b"<":
            return ".xml"
    return ".txt"


def _sniff_font(h):
    if h[:4] == b"OTTO":
        return ".otf"
    if h[:4] in (b"\0\1\0\0", b"true"):
        return ".ttf"
    if h[:4] == b"ttcf":
        return ".ttc"
    return ".bin"


def _fmtname(f):
    names = {2: "argb4444", 7: "rgb565", 9: "r16", 13: "rgba4444", 62: "rg16", 28: "crunch", 29: "crunch",
             64: "crunch", 65: "crunch", 34: "etc", 60: "etc", 61: "etc"}
    if f in names:
        return names[f]
    if 15 <= f <= 20 or f == 22:
        return "float"
    if 30 <= f <= 33:
        return "pvrtc"
    if 41 <= f <= 47:
        return "etc2"
    if 48 <= f <= 59 or 66 <= f <= 71:
        return "astc"
    return "f%d" % f


def round_half_away(f):
    return int(f + 0.5) if f >= 0 else -int(-f + 0.5)


class Opaque(int):
    """What an entry holds when it cannot be computed here (a decoder is missing): its size only"""


_T2D = [False]


def _t2d():
    """texture2ddecoder (pip install texture2ddecoder; or a folder given in TEXTURE2DDECODER_PATH): the independent
    decoder of ETC/ASTC/crunch used to check the driver; None when it is not there"""
    if _T2D[0] is False:
        try:
            if os.environ.get("TEXTURE2DDECODER_PATH"):
                sys.path.insert(0, os.environ["TEXTURE2DDECODER_PATH"])
            import texture2ddecoder
            _T2D[0] = texture2ddecoder
        except Exception:
            _T2D[0] = None
    return _T2D[0]


ASTC_SIZE = {48: 4, 49: 5, 50: 6, 51: 8, 52: 10, 53: 12, 54: 4, 55: 5, 56: 6, 57: 8, 58: 10, 59: 12,
             66: 4, 67: 5, 68: 6, 69: 8, 70: 10, 71: 12}
ETC_BYTES = {34: 8, 60: 8, 45: 8, 46: 8, 47: 16, 61: 16, 41: 8, 42: 8, 43: 16, 44: 16}


def _tga32(w, h, bgra):
    return struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 32, 8) + bgra


def _num(f):
    """a number in a field of 13 characters (as drv_unity_mesh.pas writes it)"""
    d = max(-99999.999999, min(99999.999999, float(f)))
    v = round(d * 1000000.0)
    a = ("-" if v < 0 else "") + "%d.%06d" % (abs(v) // 1000000, abs(v) % 1000000)
    return " " * (13 - len(a)) + a


def _comp(b, fmt):
    import struct as st
    if fmt == 0:
        v, = st.unpack("<f", b)
        return 0.0 if v != v or v in (float("inf"), float("-inf")) else v
    if fmt == 1:
        v, = st.unpack("<e", b)
        return 0.0 if v != v or v in (float("inf"), float("-inf")) else v
    if fmt == 2: return b[0] / 255.0
    if fmt == 3: return max(-1.0, st.unpack("<b", b)[0] / 127.0)
    if fmt == 4: return st.unpack("<H", b)[0] / 65535.0
    if fmt == 5: return max(-1.0, st.unpack("<h", b)[0] / 32767.0)
    if fmt == 6: return float(b[0])
    if fmt == 7: return float(st.unpack("<b", b)[0])
    if fmt == 8: return float(st.unpack("<H", b)[0])
    if fmt == 9: return float(st.unpack("<h", b)[0])
    if fmt == 10: return float(st.unpack("<I", b)[0])
    return float(st.unpack("<i", b)[0])


_FSIZE = {0: 4, 1: 2, 2: 1, 3: 1, 4: 2, 5: 2, 6: 1, 7: 1, 8: 2, 9: 2, 10: 4, 11: 4}


def mesh_obj(sf, t):
    """(name, OBJ bytes) of a Mesh, or None when the driver does not export it"""
    major = int(sf.unity.split(".")[0]) if sf.unity[:1].isdigit() else 0
    if major == 0 and ENGINE[0][:1].isdigit(): major = int(ENGINE[0].split(".")[0])
    if major == 0 and not sf.unity.startswith("0.0.0"): major = 0
    if major == 0: major = 2019 if sf.ver >= 19 else 2018
    if major < 2018 or t.get("m_MeshCompression", 0) != 0: return None
    vd = t.get("m_VertexData") or {}
    vc = vd.get("m_VertexCount", 0)
    data, idxb = vd.get("m_DataSize"), t.get("m_IndexBuffer")
    chans = vd.get("m_Channels")
    if not (1 <= vc <= 2000000) or not isinstance(data, bytes) or not data or not isinstance(idxb, bytes) or not idxb: return None
    if "m_IndexFormat" not in t or not isinstance(chans, list) or len(chans) < 14: return None
    idx32 = t["m_IndexFormat"] != 0
    chans = [dict(c, format=({0: 0, 1: 1, 2: 2, 3: 6, 4: 10}.get(c["format"], -1) if major < 2019 else c["format"]), dimension=c["dimension"] & 15) for c in chans[:14]]
    subs = t.get("m_SubMeshes") or []
    if len(subs) > 64: return None
    for s in subs:
        if s["firstByte"] < 0 or s["indexCount"] < 0 or s["firstByte"] + s["indexCount"] * (4 if idx32 else 2) > len(idxb): return None
    # layout of the streams
    stride = [0] * 4
    for st in range(4):
        sm = end = 0
        for c in chans:
            if c["dimension"] > 0 and c["stream"] == st:
                sz = _FSIZE.get(c["format"], 0) * c["dimension"]
                if sz == 0 or c["dimension"] > 4: return None
                sm += sz; end = max(end, c["offset"] + sz)
        stride[st] = max(sm, end)
    if any(c["dimension"] > 0 and c["stream"] > 3 for c in chans): return None
    spos, pos = [], 0
    for st in range(4):
        spos.append(pos); pos += stride[st] * vc; pos = (pos + 15) & ~15
    P, N, U = chans[0], chans[1], chans[4]
    used = {c["stream"] for c in chans if c["dimension"] > 0 and c["dimension"] <= 4}
    ok = P["dimension"] >= 3 and P["stream"] in used and len(data) >= spos[3] and spos[P["stream"]] + stride[P["stream"]] * vc <= len(data)
    if ok and N["dimension"]: ok = spos[N["stream"]] + stride[N["stream"]] * vc <= len(data)
    if ok and U["dimension"]: ok = spos[U["stream"]] + stride[U["stream"]] * vc <= len(data)
    if not ok: return None
    width = len(str(vc))

    def vec(c, v, k):
        if k >= c["dimension"]: return 0.0
        a = spos[c["stream"]] + v * stride[c["stream"]] + c["offset"] + k * _FSIZE[c["format"]]
        return _comp(data[a:a + _FSIZE[c["format"]]], c["format"])

    name = sanitize(t.get("m_Name") if isinstance(t.get("m_Name"), bytes) else b"")
    out = ["o %s\n" % name]
    for i in range(vc): out.append("v %s %s %s\n" % (_num(-vec(P, i, 0)), _num(vec(P, i, 1)), _num(vec(P, i, 2))))
    if U["dimension"]:
        for i in range(vc): out.append("vt %s %s\n" % (_num(vec(U, i, 0)), _num(vec(U, i, 1))))
    if N["dimension"]:
        for i in range(vc): out.append("vn %s %s %s\n" % (_num(-vec(N, i, 0)), _num(vec(N, i, 1)), _num(vec(N, i, 2))))

    def one(x):
        x = "%0*d" % (width, x + 1)
        if U["dimension"] and N["dimension"]: return " %s/%s/%s" % (x, x, x)
        if U["dimension"]: return " %s/%s" % (x, x)
        if N["dimension"]: return " %s//%s" % (x, x)
        return " " + x

    def tri(a, b, c): return "f" + one(c) + one(b) + one(a) + "\n"
    sz = 4 if idx32 else 2
    for si, s in enumerate(subs):
        def ix(k):
            v = int.from_bytes(idxb[s["firstByte"] + k * sz:s["firstByte"] + (k + 1) * sz], "little") + s["baseVertex"]
            return v if 0 <= v < vc else 0
        faces = []
        if s["topology"] == 0:
            faces = [tri(ix(3 * k), ix(3 * k + 1), ix(3 * k + 2)) for k in range(s["indexCount"] // 3)]
        elif s["topology"] == 1:
            for k in range(max(0, s["indexCount"] - 2)):
                a, b, c = ix(k), ix(k + 1), ix(k + 2)
                faces.append(tri(a, b, c) if k % 2 == 0 else tri(b, a, c))
        elif s["topology"] == 2:
            for k in range(s["indexCount"] // 4):
                a, b, c, d = (ix(4 * k + j) for j in range(4))
                faces += [tri(a, b, c), tri(a, c, d)]
        if faces:
            out.append("g sub%d\n" % si); out += faces
    return name, "".join(out).encode("latin-1")


def sprite_pixels(fmt, w, h, data):
    """w x h BGRA pixels (rows as stored); None when the driver does not read the format for sprites"""
    t = _t2d()
    bpp = {1: 1, 63: 1, 3: 3, 4: 4, 5: 4, 14: 4, 2: 2, 13: 2, 7: 2}.get(fmt, 0)
    if bpp:
        if w * h * bpp > len(data): return None
        px = data[:w * h * bpp]; out = bytearray()
        for i in range(w * h):
            c = px[i * bpp:(i + 1) * bpp]
            if fmt == 1: out += bytes((255, 255, 255, c[0]))
            elif fmt == 63: out += bytes((c[0], c[0], c[0], 255))
            elif fmt == 3: out += bytes((c[2], c[1], c[0], 255))
            elif fmt == 4: out += bytes((c[2], c[1], c[0], c[3]))
            elif fmt == 5: out += bytes((c[3], c[2], c[1], c[0]))
            elif fmt == 14: out += bytes(c)
            else:
                v = c[0] | c[1] << 8
                if fmt == 2: out += bytes(((v & 15) * 17, (v >> 4 & 15) * 17, (v >> 8 & 15) * 17, (v >> 12) * 17))
                elif fmt == 13: out += bytes(((v >> 4 & 15) * 17, (v >> 8 & 15) * 17, (v >> 12) * 17, (v & 15) * 17))
                else:
                    b5, g6, r5 = v & 31, v >> 5 & 63, v >> 11
                    out += bytes(((b5 << 3) | (b5 >> 2), (g6 << 2) | (g6 >> 4), (r5 << 3) | (r5 >> 2), 255))
        return bytes(out)
    blk = {10: 8, 12: 16}.get(fmt) or ETC_BYTES.get(fmt) or (16 if fmt in ASTC_SIZE else 0)
    bs = ASTC_SIZE.get(fmt, 4)
    if fmt in (28, 29):
        if not (len(data) > 74 and data[:2] == b"Hx"): return None
        cw, ch, cf = struct.unpack(">H", data[12:14])[0], struct.unpack(">H", data[14:16])[0], data[18]
        if (cw, ch) != (w, h) or not ((fmt == 28) == (cf == 0)) or not (cf == 0 or 2 <= cf <= 6): return None
        if t is None: return Opaque(w * h * 4)
        blocks = t.unpack_unity_crunch(data)
        return bytes(t.decode_bc1(blocks, w, h) if fmt == 28 else t.decode_bc3(blocks, w, h))
    if not blk: return None
    if ((w + bs - 1) // bs) * ((h + bs - 1) // bs) * blk > len(data): return None
    if t is None: return Opaque(w * h * 4)
    need = ((w + bs - 1) // bs) * ((h + bs - 1) // bs) * blk
    d = data[:need]
    if fmt == 10: img = t.decode_bc1(d, w, h)
    elif fmt == 12: img = t.decode_bc3(d, w, h)
    elif fmt in ASTC_SIZE:
        img = bytearray(t.decode_astc(d, w, h, bs, bs))
        if 48 <= fmt <= 53: img[3::4] = b"\xff" * (len(img) // 4)
    else:
        f = {34: t.decode_etc1, 60: t.decode_etc1, 45: t.decode_etc2, 46: t.decode_etc2a1, 47: t.decode_etc2a8, 61: t.decode_etc2a8,
             41: t.decode_eacr, 42: t.decode_eacr_signed, 43: t.decode_eacrg, 44: t.decode_eacrg_signed}[fmt]
        img = f(d, w, h)
    return bytes(img)


def texture_file(fmt, w, h, data, crunch=True):
    """(extension, bytes) of an extracted texture"""
    if 1 <= w <= 16384 and 1 <= h <= 16384:
        if fmt in (2, 13, 7) and w * h * 2 <= len(data):
            px = data[:w * h * 2]
            out = bytearray()
            for i in range(0, len(px), 2):
                v = px[i] | px[i + 1] << 8
                if fmt == 2:
                    out += bytes(((v & 15) * 17, (v >> 4 & 15) * 17, (v >> 8 & 15) * 17, (v >> 12) * 17))
                elif fmt == 13:
                    out += bytes(((v >> 4 & 15) * 17, (v >> 8 & 15) * 17, (v >> 12) * 17, (v & 15) * 17))
                else:
                    b5, g6, r5 = v & 31, v >> 5 & 63, v >> 11 & 31
                    out += bytes(((b5 << 3) | (b5 >> 2), (g6 << 2) | (g6 >> 4), (r5 << 3) | (r5 >> 2)))
            bpp = 3 if fmt == 7 else 4
            hdr = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, bpp * 8, 8 if bpp == 4 else 0)
            return ".tga", hdr + bytes(out)
        if fmt in ETC_BYTES or fmt in ASTC_SIZE:
            bs = ASTC_SIZE.get(fmt, 4)
            need = ((w + bs - 1) // bs) * ((h + bs - 1) // bs) * (16 if fmt in ASTC_SIZE else ETC_BYTES[fmt])
            if need <= len(data):
                t = _t2d()
                if t is None:
                    return ".tga", Opaque(18 + w * h * 4)
                blk = data[:need]
                if fmt in ASTC_SIZE:
                    img = bytearray(t.decode_astc(blk, w, h, bs, bs))
                    if fmt in range(48, 54):
                        img[3::4] = b"\xff" * (len(img) // 4)
                else:
                    f = {34: t.decode_etc1, 60: t.decode_etc1, 45: t.decode_etc2, 46: t.decode_etc2a1, 47: t.decode_etc2a8,
                         61: t.decode_etc2a8, 41: t.decode_eacr, 42: t.decode_eacr_signed, 43: t.decode_eacrg,
                         44: t.decode_eacrg_signed}[fmt]
                    img = f(blk, w, h)
                return ".tga", _tga32(w, h, bytes(img))
        if crunch and fmt in (28, 29) and len(data) > 74 and data[:2] == b"Hx":
            cw, ch, cf = struct.unpack(">H", data[12:14])[0], struct.unpack(">H", data[14:16])[0], data[18]
            if (cw, ch) == (w, h) and ((fmt == 28) == (cf == 0)) and (cf == 0 or 2 <= cf <= 6):
                bb = 8 if fmt == 28 else 16
                need = ((w + 3) // 4) * ((h + 3) // 4) * bb
                t = _t2d()
                four = b"DXT1" if fmt == 28 else b"DXT5"
                pf = struct.pack("<II4s5I", 32, 4, four, 0, 0, 0, 0, 0)
                hdr = b"DDS " + struct.pack("<7I", 124, 0x81007, h, w, need, 0, 1) + b"\0" * 44 + pf + \
                    struct.pack("<5I", 0x1000, 0, 0, 0, 0)
                if t is None:
                    return ".dds", Opaque(128 + need)
                return ".dds", hdr + t.unpack_unity_crunch(data)
    if 1 <= w <= 16384 and 1 <= h <= 16384:
        bpp = {1: 1, 63: 1, 3: 3, 4: 4, 5: 4, 14: 4}.get(fmt, 0)
        if bpp and w * h * bpp <= len(data):
            px = data[:w * h * bpp]
            if fmt == 4:
                px = b"".join(px[i + 2:i + 3] + px[i + 1:i + 2] + px[i:i + 1] + px[i + 3:i + 4] for i in range(0, len(px), 4))
            elif fmt == 5:
                px = b"".join(px[i + 3:i + 4] + px[i + 2:i + 3] + px[i + 1:i + 2] + px[i:i + 1] for i in range(0, len(px), 4))
            elif fmt == 3:
                px = b"".join(px[i + 2:i + 3] + px[i + 1:i + 2] + px[i:i + 1] for i in range(0, len(px), 3))
            hdr = struct.pack("<BBBHHBHHHHBB", 0, 0, 3 if bpp == 1 else 2, 0, 0, 0, 0, 0, w, h, bpp * 8,
                              8 if bpp == 4 else 0)
            return ".tga", hdr + px
        bb = {10: 8, 26: 8, 12: 16, 27: 16, 24: 16, 25: 16}.get(fmt, 0)
        if bb:
            need = ((w + 3) // 4) * ((h + 3) // 4) * bb
            if need <= len(data):
                four = {10: b"DXT1", 12: b"DXT5", 26: b"BC4U", 27: b"BC5U"}.get(fmt, b"DX10")
                pf = struct.pack("<II4s5I", 32, 4, four, 0, 0, 0, 0, 0)
                hdr = b"DDS " + struct.pack("<7I", 124, 0x81007, h, w, need, 0, 1) + b"\0" * 44 + pf + \
                    struct.pack("<5I", 0x1000, 0, 0, 0, 0)
                if four == b"DX10":
                    hdr += struct.pack("<5I", 95 if fmt == 24 else 98, 3, 0, 1, 0)
                return ".dds", hdr + data[:need]
    return "." + _fmtname(fmt) + ".bin", data


def sanitize(b):
    t = "".join("_" if c in "/\\:" or ord(c) < 32 else c for c in b.decode("latin-1"))
    t = t.strip(" \t\r\n\x0b\x0c\x00")
    return t or "unnamed"


def objects_of(sf, n, stream, prefix):
    """[(name, size, md5hex, data)] for the textures, sounds, texts and fonts of a Serialized `sf`;
    stream(path, offset, size) gives the bytes of a data stream (None when it is not there)"""
    out = []
    used = set()
    for o in sf.objs:
        cls = o[3]
        folder = {28: "Texture2D", 49: "TextAsset", 83: "AudioClip", 128: "Font"}.get(cls)
        if not folder:
            continue
        try:
            t = sf.read(o)
        except Exception as e:
            SKIPS["class %d read failed: %s" % (cls, e)] += 1
            continue
        ext = None
        if cls == 28:
            sd = t.get("m_StreamData") or {}
            data = None
            if sd.get("size", 0) > 0:
                data = stream(sd["path"].decode("latin-1"), sd.get("offset", 0), sd["size"])
            if data is None:
                im = t.get("image data")
                if isinstance(im, bytes) and len(im) > 0:
                    data = im
            if data is None:
                SKIPS["class 28 without data (streamed=%s)" % bool(sd.get("size", 0))] += 1
                continue
            ext, data = texture_file(t.get("m_TextureFormat", 0), t.get("m_Width", 0), t.get("m_Height", 0), data)
        elif cls == 83:
            r = t.get("m_Resource") or {}
            data = None
            if r.get("m_Size", 0) > 0:
                data = stream(r["m_Source"].decode("latin-1"), r["m_Offset"], r["m_Size"])
            if data is None:
                a = t.get("m_AudioData")
                if isinstance(a, bytes) and len(a) > 0:
                    data = a
            if data is None:
                SKIPS["class 83 without data"] += 1
                continue
            ext = _sniff_audio(data[:4])
        else:
            data = t.get("m_Script" if cls == 49 else "m_FontData")
            if not isinstance(data, bytes):
                continue
            if cls == 49:
                ext = _sniff_text(data[:256], len(data))
            else:
                if not data:
                    continue
                ext = _sniff_font(data[:256])
        raw_name = t.get("m_Name")
        nm = sanitize(raw_name if isinstance(raw_name, bytes) else b"")
        key = (folder + "/" + nm + ext).lower()
        if key in used:
            nm = nm + "_" + str(o[0])
        used.add((folder + "/" + nm + ext).lower())
        SKIPS["listed class %d" % cls] += 1
        if ext.endswith(".bin") and cls == 28:
            SKIPS["texture left raw: %s" % ext] += 1
        out.append((prefix + folder + "/" + nm + ext, int(data) if isinstance(data, Opaque) else len(data), None if isinstance(data, Opaque) else hashlib.md5(data).hexdigest(), data))
    byid = {o[0]: o for o in sf.objs}

    def emit(folder, nm, ext, data, pid):
        key = (folder + "/" + nm + ext).lower()
        if key in used:
            nm = nm + "_" + str(pid)
        used.add((folder + "/" + nm + ext).lower())
        out.append((prefix + folder + "/" + nm + ext, int(data) if isinstance(data, Opaque) else len(data), None if isinstance(data, Opaque) else hashlib.md5(data).hexdigest(), data))

    def tex_bytes(t):
        sd = t.get("m_StreamData") or {}
        if sd.get("size", 0) > 0:
            d = stream(sd["path"].decode("latin-1"), sd.get("offset", 0), sd["size"])
            if d is not None: return d
        im = t.get("image data")
        return im if isinstance(im, bytes) and len(im) > 0 else None

    for o in sf.objs:
        cls = o[3]
        if cls not in (89, 213, 43):
            continue
        try:
            t = sf.read(o)
        except Exception:
            continue
        if cls == 43:
            obj = mesh_obj(sf, t)
            if obj is not None:
                emit("Mesh", obj[0], ".obj", obj[1], o[0])
            continue
        if cls == 89:
            data = tex_bytes(t)
            cis = t.get("m_CompleteImageSize", 0)
            if data is None or cis <= 0 or len(data) < 6 * cis:
                continue
            nm0 = sanitize(t.get("m_Name") if isinstance(t.get("m_Name"), bytes) else b"")
            for k, fc in enumerate(("px", "nx", "py", "ny", "pz", "nz")):
                ext, fd = texture_file(t.get("m_TextureFormat", 0), t.get("m_Width", 0), t.get("m_Height", 0), data[k * cis:(k + 1) * cis], crunch=False)
                emit("Cubemap", nm0 + "_" + fc, ext, fd, o[0])
        else:
            rd = t.get("m_RD") or {}
            tp = rd.get("texture") or {}
            if tp.get("m_FileID", 0) != 0 or tp.get("m_PathID", 0) == 0 or tp["m_PathID"] not in byid:
                continue
            to = byid[tp["m_PathID"]]
            if to[3] != 28:
                continue
            try:
                tt = sf.read(to)
            except Exception:
                continue
            data = tex_bytes(tt)
            if data is None:
                continue
            fmt, tw, th = tt.get("m_TextureFormat", 0), tt.get("m_Width", 0), tt.get("m_Height", 0)
            r = rd.get("textureRect") or {}
            rw, rh, rx, ry = (int(round_half_away(r.get(k, 0))) for k in ("width", "height", "x", "y"))
            if rw < 1 or rh < 1 or rx < 0 or ry < 0 or rx + rw > tw or ry + rh > th or tw > 16384 or th > 16384:
                continue
            img = sprite_pixels(fmt, tw, th, data)
            if img is None:
                continue
            if isinstance(img, Opaque):
                tga = Opaque(18 + rw * rh * 4)
            else:
                rows = b"".join(img[((ry + y) * tw + rx) * 4:((ry + y) * tw + rx + rw) * 4] for y in range(rh))
                tga = _tga32(rw, rh, rows)
            emit("Sprite", sanitize(t.get("m_Name") if isinstance(t.get("m_Name"), bytes) else b""), ".tga", tga, o[0])
    return out


def expected(path):
    """[(name, size, md5hex, data)] for the files and the objects of a bundle, in the order of the driver"""
    files = load(path)
    out = [(n, len(d), hashlib.md5(d).hexdigest(), d) for n, fl, d in files]

    def stream(p, off, size):
        base = p.split("/")[-1]
        for n, fl, d in files:
            if n == base and off + size <= len(d):
                return d[off:off + size]
        return None

    for n, fl, d in files:
        if len(d) < 64 or not (fl & 4 or not n.lower().endswith((".ress", ".resource"))):
            continue
        try:
            sf = Serialized(d)
        except Exception as e:
            if fl & 4:
                SKIPS["serialized file: %s" % e] += 1
            else:
                SKIPS["not a serialized file (no flag)"] += 1
            continue
        SKIPS["serialized v%d" % sf.ver] += 1
        out += objects_of(sf, n, stream, n + ".objects/")
    return out


def expected_assets(path):
    """Same for a stand-alone serialized file; the data streams are the files of its folder"""
    d = open(path, "rb").read()
    sf = Serialized(d)
    if struct.unpack(">I", d[4:8])[0] != len(d) and sf.ver < 22:
        raise ValueError("size")
    folder = os.path.dirname(os.path.abspath(path))
    names = {f.lower(): f for f in os.listdir(folder)}

    def stream(p, off, size):
        base = p.split("/")[-1]
        if base.lower() == os.path.basename(path).lower():
            data = d
        elif base.lower() in names:
            with open(os.path.join(folder, names[base.lower()]), "rb") as f:
                f.seek(off)
                got = f.read(size)
            return got if len(got) == size else None
        else:
            return None
        return data[off:off + size] if off + size <= len(data) else None

    return objects_of(sf, "", stream, "objects/")


if __name__ == "__main__":
    for i, (n, sz, md5, _) in enumerate(expected(sys.argv[1])):
        print(i, n, sz, md5)
