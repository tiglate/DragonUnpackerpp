# Dragon UnPACKer++: status of the port

Dragon UnPACKer++ is the port of Dragon UnPACKer 5 (Delphi 7, by Alexandre "Elbereth" Devilliers) to C++20 and Qt 6, with new features. `CLAUDE.md` describes the architecture and how to build and test; this file says **what is done and what is left**. Keep it updated.

The technical notes written during the port (layouts worked out from real files, what was checked and how) are in [`PORT_NOTES.md`](PORT_NOTES.md); the formats that still need a real file are in [`FORMATS_TO_VALIDATE.md`](FORMATS_TO_VALIDATE.md); the design of the interface is in [`UI_REDESIGN.md`](UI_REDESIGN.md).

## Done

- **Driver** (`driver/`, C++20, plain C ABI "DUDI 7", rules in `driver/contract/DUDI7.md`): 131 formats, all of v5 except the two below, plus formats that v5 did not have (Unity bundles and serialized files, FMOD, XNA, XACT, GameMaker, Unreal PAK and packages, Godot PCK, Chromium PAK, LFD, Moto Racer 2 BKF, Heroes 4 H4R...). Tested with a fixture per format, fuzzing, a false positive sweep and, for the formats marked as validated, real files.
- **Interface** (`app/`, Qt 6): browsing, previews, extraction as jobs, thumbnails, search, themes, command line hooks.
- **HyperRipper**: the 27 types of v5, MP3/AAC options, naming templates, HRF files (versions 1, 2, 3), statistics.
- **Converters** (`cnv_pictex`, `cnv_exttools`), **list templates** (DUHT), **translations** (11 languages, right to left included).
- **New**: file type identification from the content (`Core/FileTypes`).
- **Packaging**: `./make.sh install` and `package` (tarball with desktop file and icons).
- The Delphi sources (`legacy/v5/`) were removed once everything that made sense was ported: git tag `legacy-v5`.

- **Console ROMs** (`Nes.cpp`, `Snes.cpp`, `MegaDrive.cpp`, shared code in `codecs/RomGfx`): NES, SNES/SFC and Mega Drive/Genesis (also `.smd`) ROMs open as the ROM parts, an info text and tile sheets (TGA, gray: the palettes are in the program). `codecs/RomCompress` decodes Nemesis and Kosinski and scans Mega Drive ROMs for streams (entries `nemesis/` and `kosinski/`, raw data and sheet), and the Nintendo LZ of the Super Nintendo (`compressed/lz_*`, raw data and 2, 3 or 4 bits sheets; three spellings of the commands: Zelda 3 offsets low byte first, Super Mario World high byte first, Super Metroid (LZ5) with two more copy commands, one with the bits flipped and one from a byte offset back; the ROM keeps the spelling that decodes most streams that use copy commands: counting streams alone is not enough, the wrong spelling decodes a few extra ones by chance; found by scanning at pointer targets and after end markers, kept only in ROMs with a run of abutting streams, which removes the noise of games that do not use it; checked on Zelda 3: 213 streams, mostly the 0x600 byte blocks of that game, and Super Metroid: 268 (its logo, fonts and enemies), and Super Mario World: 53, the 0xC00 byte files, drawn as the real sprites). Tests: `tests/verify_roms.py` (every pixel against an independent decoder, round trips with the encoders of `rom_fixtures.py`). SNES sound: `codecs/Brr` finds BRR samples (the sound chip format; the games copy them as they are to the sound CPU) and extracts them as 32 kHz mono 16 bits WAV (`audio/brr_*.wav`, `_loop` when the last block loops; the loop start and the real pitch live in the sound program, not in the sample). The scan keeps chains of valid blocks that decode like sound (no clipping, smooth, not flat, ranges that change slowly, stored next to other samples or long); Super Mario World gives its 6 samples with no junk. Tested against a reference decoder written separately (`tests/rom_fixtures.py`). Pending: other SNES schemes (Donkey Kong Country, Chrono Trigger, Mega Man X... each game has its own; Final Fantasy VI's LZSS, a 2 KB ring buffer with a size header, was tried and not added: nothing marks its end and about one in five random pointer targets decodes to the size it announces, so a scan cannot tell real streams from noise without more structure, e.g. the pointer tables of the game), Enigma/Comper/Saxman, music (the sequence data and drivers are game specific: SNES N-SPC, Mega Drive SMPS), Mega Drive DAC samples, NES sound, NES CHR in other containers.

## Left on purpose (and where to find it: `git show legacy-v5:<path>`)

- The 11th Hour GJD needs `drv_default_d11d.dat`, built from the CD with `tools/d11dc`; Worms Armageddon DIR never worked in v5 (nothing to port).
- The small tools `dlngc`/`dlngcomp`/`dlngd` (language files of v5), `dlookc`/`dlookd` (themes), `dpackc`, `duppi`, `fngen`, `dus`: they belong to the plugin, theme and language systems of v5.
- `install/setup.iss` (Inno Setup): a reference for a Windows installer.
- HyperRipper reading HRF version 0 files, and the multi-thread scan (disabled in v5 too).
- The history files and the language files of v5.

Kept from `legacy/`: the HRF specification and the DUHTComp documentation (`docs/formats/`), the list of the names of the Westwood MIX files (`driver/data/mix_names.lst`).

## Checked and dropped

- **Progress callbacks of the driver** (`Dudi7_Init` host callbacks): measured and not worth it. The biggest real archive of the library (Boltgun, 5.6 GB, 112,081 entries) opens in 62 ms and the slowest Unity files (120 MB) in 180 ms, so there is nothing to report progress about while opening. The extraction already has progress as a job.
- **Direct copy by the host for formats with raw data** (`ExtractInternal=0`): extracting the 2,306 lumps of `DOOM.WAD` takes 0.66 s and is dominated by creating the files, not by the driver.

## Open

- [ ] **Validation with real files**: see `FORMATS_TO_VALIDATE.md` (about 90 formats were only seen as fixtures).
- [ ] **Continuous integration** (GitHub Actions): build the driver and the application and run `test-driver` and `test-ui` on every push.
- [ ] **Windows**: never compiled (no Windows toolchain here). The code already has `_WIN32` branches (`LoadLibraryW`, `CreateProcessW`, `%APPDATA%`, `drv_default.d6d`) and the driver CMake names the plugin for Windows, but nothing was run. `.github/workflows/ci.yml` has a `windows` job (MSYS2/UCRT64, allowed to fail) that builds the driver and the interface: read its log after the first push to see what is left; then paths with non-ASCII characters and an installer (the Inno Setup script is in `legacy-v5:install/setup.iss`).
- [ ] Packaging for Linux beyond the tarball (AppImage or Flatpak).
- [ ] Silver PAK: the unpacked data has an unknown inner layout (exposed as one `.bin`); keyed RNC files; Blood RFF 2.x alpha; FSB4 and the raw FMOD codecs (no sample to check them with).
