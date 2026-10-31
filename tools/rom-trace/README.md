# rom-trace

Development tools that build the tables of `data/rom-metadata/` (see `FORMAT.md` there). They are **not** part of the build
and are not needed to use the application.

They drive a **local, unmodified copy of Mesen2** (a GPL-3.0 emulator) through its public Lua API, headless. Mesen2 is
neither included here nor linked: download its Linux x64 release yourself, keep it outside this repository, and delete it when
you are done. `trace.lua` only uses the documented Lua API names, and no emulator code is copied.

- `trace.lua`: runs inside the emulator. Logs the DMA transfers to VRAM / CGRAM / OAM with their source, the palette and the
  background and sprite tiles in use, VRAM dumps, and presses buttons to get past the menus. Output: JSON lines.
- `run_trace.py ROM TRACE.jsonl [FRAMES]`: runs it (`MESEN=/path/to/Mesen`). Sets `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1`
  (needed on some Linux locales) and a private settings directory with a controller on port 1 and Lua file output enabled.
- `pair.py ROM TRACE.jsonl OUT.json --title NAME`: pairs the tiles of the screen with the driver's decoded streams
  (`./make.sh build driver` first) or with raw ROM bytes, and writes the table.
- `batch.py ROMDIR [FRAMES] [NAME...]`: the ten most popular games; writes the tables into the repository.

The traces are large (2 to 20 MB per game): keep them out of the repository (`TRACE_DIR`).

- `spc_trace.lua` + `spc_recipe.py ROM TABLE.json`: the music of a game (see the Music section of `data/rom-metadata/FORMAT.md`):
  follows the uploads to the sound chip, takes a snapshot of the chip when its engine is idle, finds the song banks in the
  ROM and tries the commands (with libgme, if installed, only to hear whether a command plays something).

## What the traces showed

- `TRACE_READS=1` also logs, for each transfer to VRAM, the runs of consecutive ROM bytes the CPU read before it (what a game
  decompresses). Donkey Kong Country copies its graphics with the CPU (`$2118/$2119`), not DMA; `trace.lua` logs those
  writes as batches (`"src":"cpu"`). Some batches are raw ROM copies (same number of bytes as read); others are decompressed
  from a ROM stream whose start offset the log gives. The format of those streams is not decoded yet.
- Super Mario Kart draws its main screen in Mode 7 (8 bit tiles interleaved with the map): not handled yet.
