#!/usr/bin/env python3
"""Counts what the python reference lists and, above all, what it leaves out and why, over a list of
bundles (one path per line on stdin or in a file given as argument): a quick check that "driver ==
reference" does not hide objects both of them drop. Usage: unity_stats.py [list.txt]"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import unity_ref

paths = [l.rstrip("\n") for l in (open(sys.argv[1]) if len(sys.argv) > 1 else sys.stdin) if l.strip()]
for i, p in enumerate(paths):
    try:
        unity_ref.expected(p)
    except Exception as e:
        unity_ref.SKIPS["bundle failed: %s" % e] += 1
for k, v in sorted(unity_ref.SKIPS.items()):
    print("%7d  %s" % (v, k))
