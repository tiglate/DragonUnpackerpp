#!/usr/bin/env python3
"""Requiem: Avenging Angel models: the pose of a skeletal model (animation file next to it) against the values
computed here by hand. Usage: verify_requiem.py plugin.so dudi7cli fixtures_dir"""
import math, os, subprocess, sys, tempfile
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
if r.returncode or names != ["anim_000.bin", "anim_001.bin", "pack_skel/walk.gltf", "pack_skel/walk_2_5.gltf"]:
    sys.exit("animation pack: %s %s" % (names, r.stderr))
binary = open(os.path.join(pack, "AnimFile.bin"), "rb").read()
with tempfile.TemporaryDirectory() as tmp:
    r = subprocess.run([cli, plugin, os.path.join(pack, "AnimFile.bin"), "0-3", tmp], capture_output=True, text=True, timeout=60)
    r2 = subprocess.run([cli, plugin, os.path.join(pack, "pack_skel.mdl"), "0-2", tmp], capture_output=True, text=True, timeout=60)
    if r.returncode or r2.returncode:
        sys.exit("extraction failed: " + r.stderr + r2.stderr)
    if open(os.path.join(tmp, "0_anim_000.bin"), "rb").read() + open(os.path.join(tmp, "1_anim_001.bin"), "rb").read() != binary:
        sys.exit("animation pack: the raw records are not the .bin")
    packed = open(os.path.join(tmp, "2_walk.gltf"), "rb").read()
    # the model shown with its own animations (only walk fits it) is the same glTF
    if packed != open(os.path.join(tmp, "2_pack_skel.gltf"), "rb").read():
        sys.exit("animation pack: walk.gltf differs from the glTF of the model")
    g, buf = gltf_ref.load(os.path.join(tmp, "3_walk_2_5.gltf"))
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
print("requiem animation pack: OK")
