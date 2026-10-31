#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""spc_recipe.py ROM TABLE.json [--frames N]: works out how the game loads its music into the sound chip and adds a "music"
section to the table of the game (data/rom-metadata/FORMAT.md): the blocks of the ROM that the game uploads (the engine and
the samples once, the song banks one at a time), the state of the engine after it started (a few hundred bytes of
variables, the DSP registers) and the commands (values of the CPU ports) that start each song. No byte of music is stored:
the driver rebuilds .spc files from the ROM. Mesen2 runs the game twice (the second time to take the snapshot of the chip
when it is idle after loading a bank); songs are found by trying the commands with libgme (optional, used only to hear
whether a command plays something), else only the commands seen while playing are used."""
import argparse, ctypes, json, math, os, struct, subprocess, sys, tempfile, binascii

here = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument("rom"); ap.add_argument("table")
ap.add_argument("--frames", type=int, default=4000)
ap.add_argument("--max-command", type=int, default=0x7F)
a = ap.parse_args()
mesen = os.path.abspath(os.environ.get("MESEN", "Mesen"))
rom = open(a.rom, "rb").read()
if len(rom) % 1024 == 512: rom = rom[512:]

def run(frames, spc_dir, name, wanted):
    cfg = tempfile.mkdtemp(prefix="rom-trace-cfg-"); os.makedirs(cfg + "/Mesen2")
    with open(cfg + "/Mesen2/settings.json", "w") as f:
        f.write('{"Snes": {"Port1": {"Type": "SnesController"}}, "Debug": {"ScriptWindow": {"AllowIoOsAccess": true}}}')
    ports = os.path.join(spc_dir, name + "_ports.txt")
    env = dict(os.environ, DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="1", XDG_CONFIG_HOME=cfg, TRACE_OUT=ports,
               TRACE_FRAMES=str(frames), SPC_DIR=spc_dir, SPC_NAME=name, SPC_FRAMES=",".join(str(w) for w in wanted))
    subprocess.run([mesen, "--testRunner", os.path.abspath(a.rom), os.path.join(here, "spc_trace.lua")], env=env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=900)
    return ports

def parse_transfers(events):
    """Follows the protocol of the boot ROM of the chip: handshake 0xCC, the destination in ports 2/3, data bytes in port 1 with
    a counter in port 0, a zero in port 1 to jump. Returns [(frame at the end, [(dest, bytes)], entry)]"""
    port = [0, 0, 0, 0]; transfers = []; cur = None; counter = 0; blk = None
    for frame, p, v in events:
        if cur is not None:
            if p == 0:
                if v == counter & 0xFF and blk is not None:
                    blk[1].append(port[1]); counter = (counter + 1) & 0xFF
                else:
                    dest = port[2] | port[3] << 8
                    if port[1] == 0: cur[2] = dest; cur[0] = frame; transfers.append(cur); cur = None; blk = None
                    else: blk = [dest, bytearray()]; cur[1].append(blk); counter = 0
            port[p] = v; continue
        if p == 0 and v == 0xCC and port[1] != 0:
            blk = [port[2] | port[3] << 8, bytearray()]; cur = [frame, [blk], None]; counter = 0
        port[p] = v
    return transfers

def locate(dest, length):
    """The data of a block in the ROM: the games keep the blocks as [u16 length][u16 destination][data]"""
    pat = struct.pack("<HH", length, dest); out = []; i = -1
    while True:
        i = rom.find(pat, i + 1)
        if i < 0: return out
        out.append(i + 4)

spc_dir = tempfile.mkdtemp(prefix="rom-trace-spc-")
ports_file = run(a.frames, spc_dir, "ports", [])
events = [tuple(int(x) for x in l.split()) for l in open(ports_file)]
transfers = parse_transfers(events)
print(len(transfers), "uploads to the sound chip")
if len(transfers) < 3: sys.exit("not enough uploads: is this a game with banks of songs?")
# the bank slot: the destination that gets different data in several uploads
seen = {}
for fr, blocks, entry in transfers:
    for d, b in blocks: seen.setdefault(d, set()).add(bytes(b))
slot = max((d for d in seen if len(seen[d]) >= 2), key=lambda d: len(seen[d]), default=None)
if slot is None: sys.exit("no bank slot found")
first_bank = next(i for i, (fr, bl, e) in enumerate(transfers) if all(d == slot for d, b in bl))
engine_blocks = []
for fr, blocks, entry in transfers[:first_bank]:
    for d, b in blocks:
        if d == slot: continue
        locs = locate(d, len(b))
        if not locs: sys.exit("block %x (%d bytes) is not in the ROM" % (d, len(b)))
        engine_blocks.append((locs[0], len(b), d))
entry = transfers[first_bank][2]
def bank_blocks(i):
    out = []
    for d, b in transfers[i][1]:
        locs = locate(d, len(b))
        if not locs: sys.exit("bank block %x not in the ROM" % d)
        out.append((locs[0], len(b), d))
    return out
banks = {}
for i in range(first_bank, len(transfers)):
    if all(d == slot for d, b in transfers[i][1]):
        bl = bank_blocks(i)
        banks[tuple(bl)] = None
# banks that the game did not load while it was traced: chains in the ROM that end like the known ones ("00 00" and the entry)
tail = b"\x00\x00" + struct.pack("<H", entry)
i = -1; pat = struct.pack("<H", slot)
while True:
    i = rom.find(pat, i + 1)
    if i < 0: break
    o = i - 2
    if o < 0: continue
    ln = struct.unpack_from("<H", rom, o)[0]; e = o + 4 + ln
    if ln > 0 and e + 4 <= len(rom) and rom[e:e + 4] == tail: banks.setdefault(((o + 4, ln, slot),), None)
banks = list(banks)
print("slot %#x, %d banks, engine blocks %s" % (slot, len(banks), [(hex(o), n, hex(d)) for o, n, d in engine_blocks]))

# the snapshot: the chip idle a few frames after the first bank was loaded
idle_frame = transfers[first_bank][0] + 4
run(idle_frame + 1, spc_dir, "idle", [idle_frame])
snap = open(os.path.join(spc_dir, "idle_%05d.spc" % idle_frame), "rb").read()
snap_ram = snap[0x100:0x10100]; snap_dsp = snap[0x10100:0x10180]
base = bytearray(65536)
first_bank_blocks = bank_blocks(first_bank)
for o, n, d in engine_blocks + first_bank_blocks: base[d:d + n] = rom[o:o + n]
low = min(d for o, n, d in engine_blocks)   # the variables of the engine live below its code
patch = []
i = 0
while i < low:
    if base[i] != snap_ram[i]:
        j = i
        while j < low and (base[j] != snap_ram[j] or any(base[k] != snap_ram[k] for k in range(j, min(j + 8, low)))): j += 1
        patch.append({"dest": i, "hex": snap_ram[i:j].hex()}); i = j
    else: i += 1
cpu = {"pc": snap[0x25] | snap[0x26] << 8, "a": snap[0x27], "x": snap[0x28], "y": snap[0x29], "psw": snap[0x2A], "sp": snap[0x2B]}
io = snap_ram[0xF0:0x100].hex()
music = {"entry": entry, "slot": slot, "engine": [{"offset": o, "length": n, "dest": d} for o, n, d in engine_blocks],
         "patch": patch, "dsp": snap_dsp.hex(), "cpu": cpu, "io": io, "banks": []}

def make_spc(bank, command):
    ram = bytearray(65536)
    for o, n, d in engine_blocks + list(bank): ram[d:d + n] = rom[o:o + n]
    for p in patch:
        b = bytes.fromhex(p["hex"]); ram[p["dest"]:p["dest"] + len(b)] = b
    ram[0xF0:0x100] = bytes.fromhex(io)
    for k in range(4): ram[0xF4 + k] = command[k]
    head = b"SNES-SPC700 Sound File Data v0.30" + bytes([26, 26, 26, 30]) + struct.pack("<HBBBBB", cpu["pc"], cpu["a"], cpu["x"], cpu["y"], cpu["psw"], cpu["sp"]) + b"\0\0"
    head += b"\0" * (0x100 - len(head))
    return head + bytes(ram) + snap_dsp + b"\0" * 64

# which commands play something
gme = None
try:
    gme = ctypes.CDLL("libgme.so.0")
    gme.gme_open_file.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p), ctypes.c_int]; gme.gme_open_file.restype = ctypes.c_char_p
    gme.gme_start_track.argtypes = [ctypes.c_void_p, ctypes.c_int]; gme.gme_start_track.restype = ctypes.c_char_p
    gme.gme_play.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]; gme.gme_play.restype = ctypes.c_char_p
    gme.gme_delete.argtypes = [ctypes.c_void_p]
except OSError:
    print("libgme is not installed: the songs are the commands seen in the trace")
def plays(spc):
    path = os.path.join(spc_dir, "probe.spc"); open(path, "wb").write(spc)
    h = ctypes.c_void_p()
    if gme.gme_open_file(path.encode(), ctypes.byref(h), 32000): return False
    gme.gme_start_track(h, 0)
    n = 3 * 32000 * 2; buf = (ctypes.c_short * n)(); gme.gme_play(h, n, buf); gme.gme_delete(h)
    return math.sqrt(sum(x * x for x in buf) / n) > 100
seen_cmds = sorted({v for fr, p, v in events if p == 2 and 0 < v < 0x80})
for bank in banks:
    songs = []
    candidates = range(1, a.max_command + 1) if gme else seen_cmds
    for c in candidates:
        if gme is None or plays(make_spc(bank, (0, 0, c, 0))): songs.append([0, 0, c, 0])
    print("bank at %#x (%d blocks): %d songs" % (bank[0][0], len(bank), len(songs)))
    music["banks"].append({"blocks": [{"offset": o, "length": n, "dest": d} for o, n, d in bank], "songs": songs})

table = json.load(open(a.table)) if os.path.exists(a.table) else {}
table["music"] = music
json.dump(table, open(a.table, "w"), indent=1)
print("written", a.table)
