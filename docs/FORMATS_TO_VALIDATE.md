# Formats that still need real example files

Every format of the driver is tested with a synthetic fixture (`driver/tests/make_fixtures.py`), fuzzed, and
swept for false positives. A fixture is built from the v5 sources or from public format descriptions, so a
misreading of the layout would pass its own test. A format counts as **validated** only when a real file from
a game was listed and extracted and the result agreed with an independent reader (`driver/tests/run_real.sh`
with `verify_real.py`, or the old Pascal driver, which no longer exists).

If you own one of these games, a file from it (a small one is enough; it is only read) is worth more than any
amount of new fixtures. Run `driver/tests/run_real.sh FILE...` on it, or open it in the application and extract
everything. Report a wrong name, size, or a file that does not match the original data.

Keep this list up to date: move a format to "Validated" when a real file has been checked, and say with which
file/game and how.

## Validated on real files (nothing to do, kept for reference)

| Format | Checked with |
|---|---|
| Quake PAK, Doom WAD, Dark Forces GOB, Blood RFF (+ encrypted entries) and ART, Descent HOG, Duke 3D / Shadow Warrior GRP | Steam/GOG copies of those games, against `verify_real.py` |
| Silver PAK (RNC) | all 308 files of the game |
| Godot PCK (pack format 1) | Primal Light |
| Chromium PAK (v5) | CrossCode |
| Unreal Engine 4 PAK (v3, v11) | Flame in the Flood, Boltgun (listing only for the 5.6 GB pack) |
| Unity bundles and serialized files (UnityFS 6 to 8, serialized 17 to 22, with and without type trees, crunch, ETC/ASTC by random blocks) | thousands of files of the library, `unity_ref.py` |
| FMOD FSB5 and banks (PCM, MPEG, Vorbis to Ogg) | 88 files of the library |
| XNA XNB (textures, sounds, fonts, LZX) | 4,629 files of the library |
| ZIP | 7 files, and 35 Hitman: Contracts `.ZIP` (444 entries, all MD5 against Python `zipfile`; they have junk after the end record, now accepted) |
| Ascaron CPR | Port Royale 2: 4 archives, 8,239 entries, no errors, magic numbers of BMP/TTF/WAV right |
| Hitman: Contracts PRM, IO TEX (`.TEX`) | the PRM and TEX files inside the Hitman: Contracts ZIPs: 35 PRM (25,029 entries), 33 TEX (10,182 entries whose offsets chain exactly up to the index) |
| Total Annihilation HPI/CCX/UFO/GP3 | 30 files of the GOG release, against the old Pascal driver |
| Heroes of Might and Magic 4 H4R (`H4r.cpp`; v5's disabled reader was wrong, layout worked out from the files) | `heroes4.h4r` (17,523 entries), `text.h4r`, `updates.h4r`: all MD5 against Python gzip; gzip method 3, stored method 1 |
| Unreal packages (UTPKG), Deus Ex | all 132 `.utx .uax .umx .u` of Deus Ex (105,262 entries); 2,526 textures converted to BMP (checked with Pillow and by eye), 151 sounds (WAV, ffprobe) and 35 music modules (Impulse Tracker) extracted; the sizes of sounds and music are exact. Still unseen: other games of the family, encrypted packages, UT2003+ objects |
| Abuse SPE/SPEC (and LVL levels), Drake Paint DPA (`AbuseSpec.cpp`, from the sources of Abuse and Abuso) | the 597 `.spe`/`.lvl` files of the Abuse installation and the savegames (23,628 entries: names, and the data of all 43 entries of `frame.spe` byte for byte against a python reader), the 9 `.dpa` files (hud, icons, dev, title: PNG signatures right). Images and tiles (`.img`, `.ftile`, `.btile`: width, height, palette indices), characters (`.chr`, the sprites: the same start, index 0 transparent; the artists' `.pcx` in their names is replaced by the extension) and particle frames (`.part`) are drawn with the palette entry of the same file (`TextureConverters`); `title.spe` checked by eye. A file with no palette entry needs the palette of Options |
| Moto Racer BKF | Moto Racer: 29 files |
| Moto Racer 2 BKF (new `Bkf2.cpp`, layout worked out from the files: v5 did not know it) | 9 files, the entries chain exactly up to the end of the file |
| SiN PAK (`*.SIN`) | SiN Gold: 2 `pak0.sin`, 4,188 entries, WAV/TGA magic numbers right |
| HoMM 3 SND | 2 files, 1,232 entries; fix: the GOG files keep garbage after the 3 letters of the extension (`.WAVV`), now cut |
| Warlords Battlecry 1, 2, 3 XCR | 122 files, 23,048 entries, WAV magic numbers right |
| Jagged Alliance 2 SLF | 22 files, 28,606 entries; fix: `Data.slf` has 0 instead of 1 in its signature |
| Postal SAK | 28 files, 4,002 entries (`res.sak` is an empty file) |
| Descent 3 HOG2 / MN3 | `d3.hog` and the mission files, the sizes add up to the end of the file; fix: the data starts at the offset of the header, v5 added 48 bytes |
| LucasArts LFD, XACT XWB, GameMaker `game.unx` | the Dark Forces Remaster, the library's XWB files and Undertale/Momodora, against the old Pascal driver |

## Fixtures only: a real file is needed

### Ported from Pascal (identical to the old Pascal driver on the fixtures, never seen on a real file)

| Format (extension) | Game(s) | Notes |
|---|---|---|
| WAD2/WAD3 (`*.WAD`) | Quake textures, Half-Life | Quake/HL `.WAD` of textures, not Doom |
| FAR (`*.FAR`) | The Sims | |
| VP (`*.VP`) | Freespace, Freespace 2 | |
| ERF (`*.ERF`) | Dragon Age: Origins | |
| DRS (`*.DRS`) | Age of Empires 2 | the Steam versions (Age of Empires II HD, Age of Mythology EE) were checked: they have no `.DRS`/`.BAR` at all, only loose files; a copy of the original games is needed |
| BIGF/BIG4 (`*.BIG`) | Command & Conquer Generals, LOTR Battle for Middle-earth | |
| BAR (`*.BAR`) | Age of Empires 3, Age of Mythology | |
| HAL (`*.HAL`) | Mortyr | |
| BUN (`*.BUN`) | Monkey Island 3 | |
| DAT (Gunlok, Nascar, Act of War EDAT) | Gunlok, Nascar Racing, Act of War | three unrelated `.DAT` formats |
| SYN (`*.SYN`) | N.I.C.E.2, Breakneck, Excessive Speed | |
| VOL (`*.VOL`) | Starsiege: Tribes | |
| POD1/POD2/POD3 (`*.POD`) | Terminal Velocity, Nocturne, BloodRayne | three versions |
| GTA3 DIR/IMG, ADF | GTA III, GTA Vice City | |
| PFF3 (`*.PFF`) | Novalogic games | |
| Spellforce PAK, ZFS, SNI, PBO, Battleforge PAK, Indiana Jones 3D GOB, DWFB, PPRES, AWF, CUB, Zanzarah PAK, KAPL, TLJ PAK, BSA, AvP RFFL, CBIN, FPK, REZ, LEMBOX, COB, EIRES, BNT2, PKR, SQH, FABLE BIGB, LTAR, BAG, OPK, OAF, TMPAK, SSA, DZIP, GZP, RCF, MTF, RFA, VFS, PKPAK, RFH, XRS, STUFF (EVE), TGP0, SFL, BW2 STUFF, NightFire 007, Ubisoft BigFile M4B | see the game names in the format list of the application | Total about 50 formats |
| Ghostbusters PAK (Tonga, LZMA) | Ghostbusters: Sanctum of Slime | LZMA decoder itself is checked against liblzma |
| Godot PCK formats 2 and 3, Chromium PAK v4 | Godot 4 games, older Chromium/NW.js | only v1 / v5 are validated |
| Unreal PAK v4 to v10 and encrypted packs | UE4 games | v3 and v11 are validated; Oodle/encrypted entries are listed, not extracted |
| Unity: LZMA-compressed bundles, PVRTC/ATC textures, ETC/ASTC textures | desktop games here have none | ETC/ASTC decoders are checked against `texture2ddecoder` on random blocks only |
| Unity: serialized versions other than 17, 19 (synthetic only), 21, 22; big endian | | |
| FSB4, other FMOD codecs | | not supported |
| XNB version 4 (XNA 3.1) textures | | listed raw |

### Ported from v5 after the switch to C++ (no old Pascal driver to compare with: my reading of v5 only)

| Format | Game(s) | Notes |
|---|---|---|
| DAWL (`*.WAD`) | Geometry Wars 3: Dimensions Evolved | the names are guessed from the entry types, as in v5; the v5 header test was wrong and was replaced |
| DTA/CNT (`*.DTA`) | Hidden & Dangerous | needs the `.CNT` next to the `.DTA` |
| CBF (`*.CBF`), plain and crypted | Vietcong, Vietcong 2 | the LZW fixture only has literal codes: back-references are untested |
| DNI (`*.DNI`) | realMyst 3D | the files of a sub directory get the whole path of the directory; v5 left the directory name out, which looked like a slip |
| F22 DAT (`*.DAT`) | F-22 Air Dominance Fighter / TAW, EF2000 | names are hashes; the RA compression is not decoded |
| Lego Star Wars DAT (`*.DAT`) | LEGO Star Wars | tree of names rebuilt as in v5 |
| PCK (`*.PCK`) | Commandos 3 | folders and the position-dependent XOR of v5; a wrong guess of the key per file type only shows on a real file |
| RES (`*.RES`, `&YA1`) | Rage of Mages 2 | folders by index in the table |
| BAG + IDX (`*.BAG`) | Nox | needs the `.IDX` next to the `.BAG`; the other variant (GABA inside the file) is the one of Emperor: Battle for Dune |
| LUG/SAD (`LiOnHeAd`) | Black & White, Black & White 2, Fable: The Lost Chapters | the music banks become one `.mpg` as in v5 |
| MRC (`*.MRC`) | The Fifth Element | a folder with a single sub folder gets its name in the path of the sub folder (v5 compared Start < End: looked like a slip) |
| ASR/STM (`Asura`, `AsuraZlb`) | Aliens vs. Predator (2010) | v5's notes and code disagree on the padding of the ASTS names and the base of the RSFL offsets: both readings are tried (v5's first), a real file would settle it |
| MIX (`*.MIX`) and PAK (`*.PAK`, `*.TLK`) of Westwood | Command & Conquer, Red Alert, Tiberian Sun | names from the 20,929 of v5's list (`codecs/MixNames`, made by `tests/gen_mix_names.py`), hash of Red Alert/Tiberian Sun tried too (v5 only used the one of C&C); encrypted MIX files not supported; the PAK has no signature |
| MOS DATAFILE2.0 (`*.XWC`, `*.XTC`) | Enclave, The Chronicles of Riddick (Butcher Bay, Dark Athena) | v5 line by line, no real file: the geometry of the texture directory (the 24 + 56 bytes) is a guess confirmed only by the fixture; the .dds files are listed with their real size, and have as many mip maps as are written (v5 wrote the header of one more) |
| FORGE (`*.FORGE`) | Assassin's Creed (first game) | the preliminary reader of v5 (disabled there); entries are only the ones its tables list |
| NRM (`*.NRM`) | Heath: The Unchosen Path | the preliminary reader of v5 (disabled there) |
| JAM2 (`*.JAM`) | Leisure Suit Larry: Magna Cum Laude | recursive folder index; an index outside the name lists refuses the entry (v5 raised an exception) |
| GJD + RL (`*.GJD`) | The 7th Guest | needs the `.RL` next to the `.GJD`; The 11th Hour is not supported (v5 needed a data file built from the CD) |
| SDT (`*.SDT`) | Dungeon Keeper 2 sounds | no signature: only tried on `.SDT` files |
| GL / PAK (`*.GL`, `*.PAK`) | Star Crusader | no signature: only tried on those extensions, the offsets must grow before the directory |

### Not ported from v5 (nothing to port or a file is missing)

| Format | Game(s) | Why |
|---|---|---|
| 11th Hour GJD | The 11th Hour | v5 needs the data file `drv_default_d11d.dat`, made from the CD with the `d11dc` tool; without it nothing can be listed |
| Worms Armageddon DIR | Worms Armageddon | the v5 reader never added an entry (its table code is commented out): the layout has to be worked out from a real file |

Everything else of `drv_default.dpr` is in the C++ driver, and `legacy/v5/` was removed (tag `legacy-v5` in git has it).
