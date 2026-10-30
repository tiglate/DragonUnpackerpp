#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""play_trace.py ROM TRACE.jsonl: opens Mesen2 with its window and trace.lua so that a person plays and the trace is recorded
(no automatic input; the file is closed when the window is closed). The settings, including the controller mapping
(Settings > Input), persist in the directory MESEN_CFG (default ./mesen-cfg). Environment: MESEN (executable)."""
import os, subprocess, sys

here = os.path.dirname(os.path.abspath(__file__))
rom, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
mesen = os.path.abspath(os.environ.get("MESEN", "Mesen"))
cfg = os.path.abspath(os.environ.get("MESEN_CFG", "mesen-cfg"))
settings = cfg + "/Mesen2/settings.json"
if not os.path.exists(settings):
    os.makedirs(cfg + "/Mesen2", exist_ok=True)
    with open(settings, "w") as f:
        f.write('{"Snes": {"Port1": {"Type": "SnesController"}}, "Debug": {"ScriptWindow": {"AllowIoOsAccess": true}}}')
env = dict(os.environ, DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="1", XDG_CONFIG_HOME=cfg, TRACE_OUT=out,
           TRACE_FRAMES="100000000", TRACE_SNAP="120", TRACE_AUTOPLAY="0", TRACE_READS="1")
# the window mode aborts with std::bad_cast (std::regex and the locale of two C++ runtimes); preloading the system one fixes it
for lib in ("/usr/lib/x86_64-linux-gnu/libstdc++.so.6", "/usr/lib64/libstdc++.so.6", "/usr/lib/libstdc++.so.6"):
    if os.path.exists(lib):
        env["LD_PRELOAD"] = lib
        break
sys.exit(subprocess.call([mesen, rom, os.path.join(here, "trace.lua")], env=env))
