#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""run_trace.py ROM TRACE.jsonl [FRAMES]: runs trace.lua on a SNES ROM in headless Mesen2 (a local, unmodified copy
that is NOT part of this repository: see README.md). Environment: MESEN (path of the executable, default ./Mesen),
TRACE_TIMEOUT (seconds, default 900)."""
import os, signal, subprocess, sys, tempfile, time

here = os.path.dirname(os.path.abspath(__file__))
rom, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
frames = sys.argv[3] if len(sys.argv) > 3 else "3000"
mesen = os.path.abspath(os.environ.get("MESEN", "Mesen"))

# private Mesen settings: a controller on port 1 (the default is none) and Lua file output (off by default)
cfg = tempfile.mkdtemp(prefix="rom-trace-cfg-")
os.makedirs(cfg + "/Mesen2")
with open(cfg + "/Mesen2/settings.json", "w") as f:
    f.write('{"Snes": {"Port1": {"Type": "SnesController"}}, "Debug": {"ScriptWindow": {"AllowIoOsAccess": true}}}')
env = dict(os.environ, DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="1", XDG_CONFIG_HOME=cfg,
           TRACE_OUT=out, TRACE_FRAMES=frames, TRACE_SNAP="120")
p = subprocess.Popen([mesen, "--testRunner", rom, os.path.join(here, "trace.lua")], env=env,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, start_new_session=True)
t0 = time.time()
try:
    log = p.communicate(timeout=int(os.environ.get("TRACE_TIMEOUT", "900")))[0]
except subprocess.TimeoutExpired:
    os.killpg(p.pid, signal.SIGKILL)
    log = p.communicate()[0] + "\n(timeout)"
size = os.path.getsize(out) if os.path.exists(out) else 0
print(f"{os.path.basename(rom)}: rc={p.returncode} {time.time() - t0:.0f}s {size} bytes")
if p.returncode or not size:
    print(log[-800:])
