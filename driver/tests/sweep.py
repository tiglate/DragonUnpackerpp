#!/usr/bin/env python3
"""False-positive sweep: runs the driver on a sample of unrelated files and
reports which ones it claims, grouped by (extension, format id) with examples,
so that a wrong claim (a game file of another format accepted as one of ours)
shows up. Files are copied (up to 20 MB each) to a temporary directory first.
Usage: sweep.py plugin.so dudi7cli root... [--max N] [--seed N]"""
import collections, os, random, shutil, subprocess, sys, tempfile

args = [a for a in sys.argv[1:] if not a.startswith("--")]
opts = dict(a[2:].split("=") for a in sys.argv[1:] if a.startswith("--") and "=" in a)
plugin, cli, roots = args[0], args[1], args[2:]
maxn = int(opts.get("max", 900))
random.seed(int(opts.get("seed", 1)))

byext = collections.defaultdict(list)
for root in roots:
    for dp, dn, fn in os.walk(root):
        if "Proton" in dp or "SteamLinux" in dp:
            continue
        for f in fn:
            p = os.path.join(dp, f)
            try:
                if 64 <= os.path.getsize(p) <= 20 * 1024 * 1024 and os.path.isfile(p) and not os.path.islink(p):
                    byext[os.path.splitext(f)[1].lower()].append(p)
            except OSError:
                pass
sample = []
for e, l in byext.items():          # a few of every extension, more of the ones that matter
    random.shuffle(l)
    sample += l[:15]
for e in (".res", ".pak", ".dat", ".snd", ".awf", ".cub", ".gob", ".bin", ".wad", ".img", ".dir", ".pod", ".vol"):
    sample += [p for p in byext.get(e, [])[:40] if p not in sample]
random.shuffle(sample)
sample = sample[:maxn]

claims = collections.defaultdict(list)
with tempfile.TemporaryDirectory() as td:
    for i, p in enumerate(sample):
        fn = os.path.join(td, "f" + os.path.splitext(p)[1])
        shutil.copy(p, fn)
        try:
            r = subprocess.run([cli, plugin, fn], capture_output=True, timeout=30)
        except subprocess.TimeoutExpired:
            print("TIMEOUT", p)
            continue
        first = r.stdout.decode(errors="replace").split("\n")[0]
        if first.startswith("#"):
            claims[(os.path.splitext(p)[1].lower(), first[1:].split("\t")[0])].append(p)
        os.remove(fn)
print("%d files, %d claimed" % (len(sample), sum(len(v) for v in claims.values())))
for (ext, fid), l in sorted(claims.items()):
    print("%-8s %-8s %3d   e.g. %s" % (ext, fid, len(l), l[0]))
