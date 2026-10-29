#!/usr/bin/env python3
"""Empty archives: for every fixture, finds a header field equal to 3 (the number of entries
of the fixtures) and sets it to 0; the driver must then open the archive with 0 entries.
Catches loops like `for I := 0 to N - 1` with an unsigned I (N = 0 wraps to 4 billion).
Formats whose signature check wants at least one entry, or whose count is not stored
as a plain field, are not in zero_entries.expected: the test only guards the ones that are.
Usage: zero_entries.py plugin.so dudi7cli fixtures_dir   (--update rewrites the expected list)"""
import os, struct, subprocess, sys, tempfile

args = [a for a in sys.argv[1:] if not a.startswith("--")]
plugin, cli, fx = args
here = os.path.dirname(os.path.abspath(__file__))
exp_path = os.path.join(here, "zero_entries.expected")
ok = set()
with tempfile.TemporaryDirectory() as tmp:
    for name in sorted(os.listdir(fx)):
        p = os.path.join(fx, name)
        if os.path.isdir(p) or name.endswith(".rnc") or name.startswith(("gta3", "dune", "payload", "dups", "corrupt")):
            continue
        data = open(p, "rb").read()
        cand = []
        for size, fmt in ((4, "<I"), (2, "<H"), (4, ">I")):
            for pos in list(range(0, min(len(data), 300))) + list(range(max(0, len(data) - 300), len(data))):
                if pos + size <= len(data) and struct.unpack_from(fmt, data, pos)[0] == 3:
                    cand.append((pos, size, fmt))
        for pos, size, fmt in cand:
            b = bytearray(data)
            struct.pack_into(fmt, b, pos, 0)
            t = os.path.join(tmp, name)
            open(t, "wb").write(b)
            r = subprocess.run([cli, plugin, t, "list"], capture_output=True, text=True, timeout=60)
            if r.returncode == 0 and r.stdout.startswith("#") and r.stdout.split("\n")[0].split("\t")[1] == "0":
                ok.add(name)
                break
if "--update" in sys.argv:
    open(exp_path, "w").write("\n".join(sorted(ok)) + "\n")
    print("%d fixtures open empty" % len(ok))
    sys.exit(0)
expected = set(open(exp_path).read().split())
missing = sorted(expected - ok)
for m in missing:
    print("FAIL: the empty version of", m, "does not open")
print("zero-entry archives: %d/%d OK" % (len(expected & ok), len(expected)))
sys.exit(1 if missing else 0)
