"""Minimal reader of the glTF files made for the Requiem models: nodes with translation and rotation, one skin
with identity inverse bind matrices, linear animation channels. Used by verify_requiem.py (independent of the
driver: the pose is recomputed here from the nodes)."""
import base64, json, struct


def load(path):
    g = json.load(open(path))
    uri = g["buffers"][0]["uri"]
    buf = base64.b64decode(uri.split(",", 1)[1])
    return g, buf


def accessor(g, buf, index):
    a = g["accessors"][index]
    v = g["bufferViews"][a["bufferView"]]
    width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
    code = {5126: "f", 5125: "I", 5121: "B"}[a["componentType"]]
    size = struct.calcsize(code)
    out = struct.unpack_from("<%d%s" % (a["count"] * width, code), buf, v["byteOffset"])
    return [out[i:i + width] for i in range(0, len(out), width)], width


def qmul(p, q):          # xyzw
    px, py, pz, pw = p
    x, y, z, w = q
    return (pw * x + px * w + py * z - pz * y, pw * y + py * w + pz * x - px * z,
            pw * z + pz * w + px * y - py * x, pw * w - px * x - py * y - pz * z)


def rot(q, v):
    x, y, z, w = q
    # v' = q v q^-1
    t = (2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]), 2 * (x * v[1] - y * v[0]))
    return (v[0] + w * t[0] + (y * t[2] - z * t[1]), v[1] + w * t[1] + (z * t[0] - x * t[2]),
            v[2] + w * t[2] + (x * t[1] - y * t[0]))


def world(g, trs):
    """world (rotation quaternion, origin) of every joint from per node (translation, rotation)"""
    nodes = g["nodes"]
    res = {}

    def visit(i, pq, po):
        t, r = trs[i]
        q = qmul(pq, r)
        d = rot(pq, t)
        o = (po[0] + d[0], po[1] + d[1], po[2] + d[2])
        res[i] = (q, o)
        for c in nodes[i].get("children", []):
            visit(c, q, o)
    for r in g["scenes"][0]["nodes"]:
        if "mesh" not in nodes[r]:
            visit(r, (0, 0, 0, 1), (0, 0, 0))
    return res


def rest_pose(g):
    return [(tuple(n["translation"]), tuple(n["rotation"])) for n in g["nodes"] if "mesh" not in n]


def animation_frame(g, buf, anim, frame):
    """node (translation, rotation) at a frame of an animation (channels sampled at their keys)"""
    trs = rest_pose(g)
    a = g["animations"][anim]
    for ch in a["channels"]:
        s = a["samplers"][ch["sampler"]]
        keys, _ = accessor(g, buf, s["output"])
        k = keys[min(frame, len(keys) - 1)]
        node = ch["target"]["node"]
        if ch["target"]["path"] == "translation":
            trs[node] = (tuple(k), trs[node][1])
        else:
            trs[node] = (trs[node][0], tuple(k))
    return trs


def skinned_positions(g, buf, trs):
    pos, _ = accessor(g, buf, g["meshes"][0]["primitives"][0]["attributes"]["POSITION"])
    jts, _ = accessor(g, buf, g["meshes"][0]["primitives"][0]["attributes"]["JOINTS_0"])
    w = world(g, trs)
    out = []
    for p, j in zip(pos, jts):
        q, o = w[j[0]]
        r = rot(q, p)
        out.append((r[0] + o[0], r[1] + o[1], r[2] + o[2]))
    return out
