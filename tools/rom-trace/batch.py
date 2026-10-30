#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""batch.py ROMDIR [FRAMES] [NAME...]: traces the games of GAMES found in ROMDIR (file names as in No-Intro sets) and writes
data/rom-metadata/snes/CRC32.json for each. Traces go to $TRACE_DIR (default ./traces), not into the repository.
Environment: MESEN (executable), see run_trace.py."""
import os, shutil, subprocess, sys

here = os.path.dirname(os.path.abspath(__file__))
repo = os.path.join(here, "..", "..")
GAMES = [
    ("smw", "Super Mario World", "Super Mario World (USA).sfc"),
    ("smk", "Super Mario Kart", "Super Mario Kart (USA).sfc"),
    ("alttp", "The Legend of Zelda: A Link to the Past", "Legend of Zelda, The - A Link to the Past (USA).sfc"),
    ("dkc", "Donkey Kong Country", "Donkey Kong Country (USA).sfc"),
    ("smetroid", "Super Metroid", "Super Metroid (Japan, USA) (En,Ja).sfc"),
    ("sf2", "Street Fighter II", "Street Fighter II (USA).sfc"),
    ("ct", "Chrono Trigger", "Chrono Trigger (USA).sfc"),
    ("ff6", "Final Fantasy III (VI)", "Final Fantasy III (USA).sfc"),
    ("yoshi", "Super Mario World 2: Yoshi's Island", "Super Mario World 2 - Yoshi_s Island (USA).sfc"),
    ("allstars", "Super Mario All-Stars", "Super Mario All-Stars (USA).sfc"),
]
romdir = sys.argv[1]
frames = sys.argv[2] if len(sys.argv) > 2 else "6000"
only = sys.argv[3:]
tdir = os.environ.get("TRACE_DIR", "traces")
os.makedirs(tdir, exist_ok=True)
for key, title, fn in GAMES:
    if only and key not in only: continue
    rom = os.path.join(romdir, fn)
    if not os.path.exists(rom):
        print(key, "missing:", rom); continue
    trace = os.path.join(tdir, key + ".jsonl")
    subprocess.run([sys.executable, "-I", os.path.join(here, "run_trace.py"), rom, trace, frames], check=False)
    tmp = os.path.join(tdir, key + ".json")
    subprocess.run([sys.executable, "-I", os.path.join(here, "pair.py"), rom, trace, tmp, "--title", title], check=False)
    if os.path.exists(tmp):
        import json
        crc = json.load(open(tmp))["crc32"]
        dst = os.path.join(repo, "data", "rom-metadata", "snes", crc + ".json")
        shutil.move(tmp, dst)
