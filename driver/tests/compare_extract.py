#!/usr/bin/env python3
"""compare_extract.py listing extracted_dir label
listing = dudi7cli output (index offset size md5 name); compares the number of files and
the multiset of MD5 (names are sanitized and made unique by the extractor, so they are not
compared except for the report of the ones that were renamed)."""
import collections, hashlib, os, sys

listing, dest, label = sys.argv[1:4]
want = collections.Counter(); names = {}
for l in open(listing, encoding="latin-1"):
    if l.startswith("#"):
        continue
    i, off, size, md5, name = l.rstrip("\n").split("\t", 4)
    want[md5] += 1
    names[name] = names.get(name, 0) + 1
got = collections.Counter(); n = 0
for dp, dn, fn in os.walk(dest):
    for f in fn:
        n += 1
        got[hashlib.md5(open(os.path.join(dp, f), "rb").read()).hexdigest()] += 1
total = sum(want.values())
dups = sum(c - 1 for c in names.values() if c > 1)
ok = got == want and n == total
print("%s: %d entries, %d files extracted, %d duplicated names, %s" % (label, total, n, dups, "OK" if ok else "MISMATCH"))
sys.exit(0 if ok else 1)
