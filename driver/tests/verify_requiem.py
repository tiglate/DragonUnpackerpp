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
print("requiem pose: OK")
