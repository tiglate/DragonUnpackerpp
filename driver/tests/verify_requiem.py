#!/usr/bin/env python3
"""Requiem: Avenging Angel models: the pose of a skeletal model (animation file next to it) against the values
computed here by hand. Usage: verify_requiem.py plugin.so dudi7cli fixtures_dir"""
import math, os, subprocess, sys, tempfile

plugin, cli, fx = sys.argv[1:4]
with tempfile.TemporaryDirectory() as tmp:
    r = subprocess.run([cli, plugin, os.path.join(fx, "req_skel.mdl"), "0-0", tmp], capture_output=True, text=True, timeout=60)
    if r.returncode:
        sys.exit("extraction failed: " + r.stderr)
    obj = open(os.path.join(tmp, "0_req_skel.obj")).read().split("\n")
if not any(l.startswith("# posed with the first frame of req_skel.ani") for l in obj):
    sys.exit("the model was not posed: " + "\n".join(obj[:4]))
v = [tuple(map(float, l.split()[1:4])) for l in obj if l.startswith("v ")]
# limb 0 is turned 90 degrees around z at (10, 0, 0); limb 1 starts at (10, 0, 0) + R * (1, 0, 0) = (10, 1, 0)
want = {0: (10, 0, 0), 1: (8, 1, 0), 3: (4, 4, 0), 4: (2, 5, 0)}      # vertices (i, 2i, 0) of the file
for i, w in want.items():
    if max(abs(a - b) for a, b in zip(v[i], w)) > 1e-4:
        sys.exit("vertex %d: %s, expected %s" % (i, v[i], w))
print("requiem pose: OK")
