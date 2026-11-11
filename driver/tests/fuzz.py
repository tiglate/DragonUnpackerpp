#!/usr/bin/env python3
"""Mutates the fixtures (random bytes, truncation, extreme header values) and
checks that the driver answers with a status code: no crash, no hang, and no
unexpected exception (generic error -1 would hide a bug in a format unit).
Usage: fuzz.py plugin.so dudi7cli fixtures_dir [mutations_per_file]
(build dudi7cli with ./make.sh build driver, make the fixtures with make_fixtures.py)"""
import collections, glob, os, random, subprocess, sys, tempfile

plugin, cli, fixtures = sys.argv[1:4]
count = int(sys.argv[4]) if len(sys.argv) > 4 else 120
random.seed(42)
EXTREME = [b"\xff\xff\xff\xff", b"\xff\xff\xff\x7f", b"\x00\x00\x00\x80", b"\x01\x00\x00\x00"]
problems = []
runs = 0
with tempfile.TemporaryDirectory() as td:
    for src in sorted(glob.glob(os.path.join(fixtures, "*"))):
        if src.endswith(".bin") or os.path.isdir(src):
            continue
        data = open(src, "rb").read()
        base = os.path.basename(src)
        for k in range(count):
            d = bytearray(data)
            if k % 4 == 0:
                for _ in range(random.randint(1, 6)):
                    d[random.randrange(len(d))] = random.randrange(256)
            elif k % 4 == 1:
                d = d[:random.randrange(len(d))]
            elif k % 4 == 2:
                for _ in range(random.randint(1, 4)):
                    d[random.randrange(min(len(d), 300))] = random.choice([0, 255, 128, random.randrange(256)])
            else:
                for _ in range(2):
                    p = random.randrange(0, max(1, min(len(d), 300) - 4))
                    d[p:p + 4] = random.choice(EXTREME)
            fn = os.path.join(td, base)       # keep the name: some formats need their extension
            open(fn, "wb").write(d)
            try:
                r = subprocess.run([cli, plugin, fn], capture_output=True, timeout=20)
            except subprocess.TimeoutExpired:
                problems.append((base, k, "timeout"))
                continue
            runs += 1
            err = r.stderr.decode(errors="replace")
            if r.returncode not in (0, 1):
                problems.append((base, k, "exit %d" % r.returncode))
            elif "code -1)" in err:
                problems.append((base, k, err.strip().splitlines()[1][:100]))
print("%d runs, %d problem(s)" % (runs, len(problems)))
for p in problems[:15]:
    print(" ", p)
sys.exit(1 if problems else 0)
