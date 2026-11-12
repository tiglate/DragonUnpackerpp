#!/usr/bin/env python3
"""Requiem: Avenging Angel models: the pose of a skeletal model (animation file next to it) against the values
computed here by hand. Usage: verify_requiem.py plugin.so dudi7cli fixtures_dir"""
import math, os, struct, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

plugin, cli, fx = sys.argv[1:4]
with tempfile.TemporaryDirectory() as tmp:
    r = subprocess.run([cli, plugin, os.path.join(fx, "req_skel.mdl"), "0-0", tmp], capture_output=True, text=True, timeout=60)
    if r.returncode:
        sys.exit("extraction failed: " + r.stderr)
    obj = open(os.path.join(tmp, "0_req_skel.obj")).read().split("\n")
if not any(l.startswith("# posed with the first frame of req_skel") for l in obj):
    sys.exit("the model was not posed: " + "\n".join(obj[:4]))
v = [tuple(map(float, l.split()[1:4])) for l in obj if l.startswith("v ")]
# limb 0 is turned 90 degrees around z at (10, 0, 0); limb 1 starts at (10, 0, 0) + R * (1, 0, 0) = (10, 1, 0)
want = {0: (10, 0, 0), 1: (9, 1, 0), 3: (1, 4, 0), 4: (-6, 5, 0)}      # vertices (i, i * i, 0) of the file
for i, w in want.items():
    if max(abs(a - b) for a, b in zip(v[i], w)) > 1e-4:
        sys.exit("vertex %d: %s, expected %s" % (i, v[i], w))

# the glTF: the skinned positions at the rest pose and at every frame of the animation must be the corners of the
# faces of the OBJ (computed here from the nodes, not by the driver)
import gltf_ref
with tempfile.TemporaryDirectory() as tmp:
    r = subprocess.run([cli, plugin, os.path.join(fx, "req_skel.mdl"), "0-2", tmp], capture_output=True, text=True, timeout=60)
    if r.returncode:
        sys.exit("extraction failed: " + r.stderr)
    g, buf = gltf_ref.load(os.path.join(tmp, "2_req_skel.gltf"))
corners = []
for l in obj:
    if l.startswith("f "):
        corners += [int(x.split("/")[0]) - 1 for x in l.split()[1:]]
got = gltf_ref.skinned_positions(g, buf, gltf_ref.rest_pose(g))
if len(got) != len(corners):
    sys.exit("gltf has %d vertices, the faces of the obj %d corners" % (len(got), len(corners)))
for k, c in enumerate(corners):
    if max(abs(a - b) for a, b in zip(got[k], v[c])) > 1e-4:
        sys.exit("gltf corner %d: %s, obj %s" % (k, got[k], v[c]))
if len(g["animations"]) != 1:
    sys.exit("expected 1 animation, got %d" % len(g["animations"]))

# Models3.txt: the slot of a static model (S, then the O line is another slot) and of a gun (G)
with tempfile.TemporaryDirectory() as tmp:
    for name, slot in (("req_static", 3065), ("req_skel", 3059)):
        r = subprocess.run([cli, plugin, os.path.join(fx, name + ".mdl"), "0-2", tmp], capture_output=True, text=True, timeout=60)
        if r.returncode:
            sys.exit("extraction failed: " + r.stderr)
        mtl = open(os.path.join(tmp, "1_%s.mtl" % name)).read()
        if "map_Kd tex_%04d.tga" % slot not in mtl:
            sys.exit("%s: the material does not name slot %d: %s" % (name, slot, mtl))
        extras = gltf_ref.load(os.path.join(tmp, "2_%s.gltf" % name))[0]["materials"][0]["extras"]
        if extras["artSlot"] != slot:
            sys.exit("%s: glTF slot %s, expected %d" % (name, extras["artSlot"], slot))
        if name == "req_static" and extras.get("otherSlots") != [3206]:
            sys.exit("req_static: other slots %s" % extras.get("otherSlots"))
print("requiem pose: OK")

# The animation pack (AnimFile.ary/.bin/.tab + Models3.txt): the raw records as they are, then each animation that
# fits its model as a glTF; record 0 fits nothing, animation numbers 39 (record 0) and 40 (past the table) give none
pack = os.path.join(fx, "req_pack")
r = subprocess.run([cli, plugin, os.path.join(pack, "AnimFile.ary"), "list"], capture_output=True, text=True, timeout=60)
names = [l.split("\t")[4] for l in r.stdout.splitlines()[1:]]
if r.returncode or names != ["raw/000_junk.ani", "raw/001_walk.ani", "raw/002_filled.ani", "raw/003_wrongmodel.ani", "raw/004.ani",
                              "pack_skel/walk.gltf", "pack_skel/walk_2_5.gltf", "pack_skel/filled.gltf", "solo/wrongmodel.gltf"]:
    sys.exit("animation pack: %s %s" % (names, r.stderr))
binary = open(os.path.join(pack, "AnimFile.bin"), "rb").read()
with tempfile.TemporaryDirectory() as tmp:
    r = subprocess.run([cli, plugin, os.path.join(pack, "AnimFile.bin"), "0-8", tmp], capture_output=True, text=True, timeout=60)
    r2 = subprocess.run([cli, plugin, os.path.join(pack, "pack_skel.mdl"), "0-2", tmp], capture_output=True, text=True, timeout=60)
    if r.returncode or r2.returncode:
        sys.exit("extraction failed: " + r.stderr + r2.stderr)
    raw = ("000_junk", "001_walk", "002_filled", "003_wrongmodel", "004")
    if b"".join(open(os.path.join(tmp, "%d_%s.ani" % (i, n)), "rb").read() for i, n in enumerate(raw)) != binary:
        sys.exit("animation pack: the raw records are not the .bin")
    # each glTF of the pack poses the model as the animation of the same name in the model's own glTF
    own, ownbuf = gltf_ref.load(os.path.join(tmp, "2_pack_skel.gltf"))
    ownNames = [a.get("name") for a in own["animations"]]
    if ownNames != ["walk", "filled"]:
        sys.exit("animation pack: the model's own animations are %s" % ownNames)
    for file, label in (("5_walk.gltf", "walk"), ("7_filled.gltf", "filled")):
        pg, pbuf = gltf_ref.load(os.path.join(tmp, file))
        a = gltf_ref.skinned_positions(pg, pbuf, gltf_ref.animation_frame(pg, pbuf, 0, 0))
        b = gltf_ref.skinned_positions(own, ownbuf, gltf_ref.animation_frame(own, ownbuf, ownNames.index(label), 0))
        if len(a) != len(b) or any(max(abs(x - y) for x, y in zip(p, q)) > 1e-4 for p, q in zip(a, b)):
            sys.exit("animation pack: %s differs from the animation %s of the model" % (file, label))
    g, buf = gltf_ref.load(os.path.join(tmp, "6_walk_2_5.gltf"))
    gf, buff = gltf_ref.load(os.path.join(tmp, "7_filled.gltf"))
    gs, bufs = gltf_ref.load(os.path.join(tmp, "8_wrongmodel.gltf"))
    pobj = open(os.path.join(tmp, "0_pack_skel.obj")).read().split("\n")
if not any(l.startswith("# posed with the first frame of walk") for l in pobj):
    sys.exit("pack_skel.mdl was not posed by the pack: " + "\n".join(pobj[:3]))
pv = [tuple(map(float, l.split()[1:4])) for l in pobj if l.startswith("v ")]
pc = [int(x.split("/")[0]) - 1 for l in pobj if l.startswith("f ") for x in l.split()[1:]]
got = gltf_ref.skinned_positions(g, buf, gltf_ref.animation_frame(g, buf, 0, 0))
if len(got) != len(pc) or any(max(abs(a - b) for a, b in zip(got[k], pv[c])) > 1e-4 for k, c in enumerate(pc)):
    sys.exit("animation pack: the first frame of the glTF is not the posed model")
if g["materials"][0]["extras"]["artSlot"] != 3059 or g["animations"][0].get("name") != "walk_2_5":
    sys.exit("animation pack: slot %s, animation %s" % (g["materials"][0]["extras"]["artSlot"], g["animations"][0].get("name")))
# the 0xCDCDCDCD fill in place of the position of limb 0 is no position: limb 0 at the origin, limb 1 at R * (1, 0, 0)
fw = gltf_ref.world(gf, gltf_ref.animation_frame(gf, buff, 0, 0))
for joint, want in ((0, (0, 0, 0)), (1, (0, 1, 0))):
    if max(abs(a - b) for a, b in zip(fw[joint][1], want)) > 1e-4:
        sys.exit("animation pack: the filled position gives limb %d at %s, expected %s" % (joint, fw[joint][1], want))
# a record listed under a model it does not fit goes to the only model with its limb count (solo.mdl, 3 limbs):
# every limb turned 90 degrees around z, one unit along x from its parent
sw = gltf_ref.world(gs, gltf_ref.animation_frame(gs, bufs, 0, 0))
for joint, want in ((0, (1, 0, 0)), (1, (1, 1, 0)), (2, (0, 1, 0))):
    if len(sw) != 3 or max(abs(a - b) for a, b in zip(sw[joint][1], want)) > 1e-4:
        sys.exit("animation pack: solo limb %d at %s, expected %s" % (joint, sw.get(joint, (0, None))[1], want))
print("requiem animation pack: OK")

# A level (.jam): level.gltf has the pieces of the cut face and the face that was not cut, not the face without
# texture; one primitive per texture (art slots 7 and 9), texture coordinates = texels / 128 with V negated
REQ_LEVEL_VERTICES = [(0, 0, 0), (10, 0, 0), (10, 0, 10), (0, 0, 10), (5, 5, 5)]     # as made by make_fixtures.py
REQ_LEVEL_PIECES = [((0, 1, 2), ((0.05, 0.05), (64, 0.05), (64, 64))), ((0, 2, 3), ((0.05, 0.05), (64, 64), (0.05, 64)))]
want = {7: REQ_LEVEL_PIECES, 9: [((0, 1, 4), ((0, 0),) * 3)]}
for version in (0, 2):
    with tempfile.TemporaryDirectory() as tmp:
        r = subprocess.run([cli, plugin, os.path.join(fx, "req_v%d.jam" % version), "5-5", tmp], capture_output=True, text=True, timeout=60)
        if r.returncode:
            sys.exit("level extraction failed: " + r.stderr)
        g, buf = gltf_ref.load(os.path.join(tmp, "5_level.gltf"))
    prims = g["meshes"][0]["primitives"]
    slots = [g["materials"][p["material"]]["extras"] for p in prims]
    if slots != [{"artSlot": 7, "uvSize": 128}, {"artSlot": 9, "uvSize": 128}]:
        sys.exit("level: materials %s" % slots)
    for prim, extras in zip(prims, slots):
        pos, _ = gltf_ref.accessor(g, buf, prim["attributes"]["POSITION"])
        uv, _ = gltf_ref.accessor(g, buf, prim["attributes"]["TEXCOORD_0"])
        tris, _ = gltf_ref.accessor(g, buf, prim["indices"])
        corners = [(REQ_LEVEL_VERTICES[i], (u / 128, -v / 128)) for idx, uvs in want[extras["artSlot"]] for i, (u, v) in zip(idx, uvs)]
        got = [(pos[t[0]], uv[t[0]]) for t in tris]
        if len(got) != len(corners) or any(max(abs(a - b) for a, b in zip(p + q, wp + wq)) > 1e-6 for (p, q), (wp, wq) in zip(got, corners)):
            sys.exit("level v%d slot %d: corners %s, expected %s" % (version, extras["artSlot"], got, corners))
    # the lightmaps: the PNG of the occlusion texture, sampled at TEXCOORD_1 of each corner (decoded here: zlib, filter 0)
    import zlib
    mat = g["materials"][prims[0]["material"]]
    if mat.get("occlusionTexture") != {"index": 0, "texCoord": 1}:
        sys.exit("level: occlusion texture %s" % mat.get("occlusionTexture"))
    view = g["bufferViews"][g["images"][g["textures"][0]["source"]]["bufferView"]]
    png = buf[view["byteOffset"]:view["byteOffset"] + view["byteLength"]]
    at, idat = 8, b""
    while at < len(png):
        length, kind = struct.unpack(">I4s", png[at:at + 8])
        body = png[at + 8:at + 8 + length]
        if kind == b"IHDR":
            width, height = struct.unpack(">II", body[:8])
        elif kind == b"IDAT":
            idat += body
        at += 12 + length
    raw = zlib.decompress(idat)
    def light(u, v):
        x, y = int(u * width), int(v * height)
        row = raw[y * (width * 3 + 1):(y + 1) * (width * 3 + 1)]
        if row[0] != 0:
            sys.exit("level: the lightmap rows are filtered")
        return tuple(row[1 + x * 3:4 + x * 3])
    want_light = {7: [(255, 0, 0)] * 3 + [(0, 255, 0)] * 3, 9: [(255, 255, 255)] * 3}
    for prim, extras in zip(prims, slots):
        luv, _ = gltf_ref.accessor(g, buf, prim["attributes"]["TEXCOORD_1"])
        tris, _ = gltf_ref.accessor(g, buf, prim["indices"])
        got = [light(*luv[t[0]]) for t in tris]
        if got != want_light[extras["artSlot"]]:
            sys.exit("level v%d slot %d: lightmap colours %s" % (version, extras["artSlot"], got))
print("requiem level: OK")
