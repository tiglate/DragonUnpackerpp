# ROM metadata tables

One JSON file per game and console, `data/rom-metadata/<console>/<crc32>.json`, named by the CRC-32 (lowercase hex) of the
ROM without its 512-byte copier header. The tables hold **metadata only**: offsets, sizes, codec names and colours. They
contain no bytes of any game.

They are produced by running the game in an emulator and recording which ROM data reaches the screen and in which palette
(`tools/rom-trace/`), so the pairing between graphics and palettes, which the ROMs do not store statically, is known.

```json
{
  "title": "Super Mario World",
  "console": "snes",
  "crc32": "b19ed489",
  "size": 524288,
  "tracedTiles": 4627,          // distinct on-screen tiles examined while tracing
  "explainedTiles": 3717,       // of them, the ones whose pixels were found in the ROM
  "streams": [                  // compressed graphics (the codec is one the driver decodes)
    {
      "codec": "lz",            // lz (Nintendo LZ), lzp (packet LZ, Chrono Trigger), lzss (FF6), ...
      "offset": 278464,         // ROM offset of the compressed stream (headerless ROM)
      "bpp": [3],               // tile formats the screen used from it
      "usedBy": ["bg1", "sprite"],
      "tilesSeen": 8,
      "palettes": [             // the palettes it was drawn with, most used first
        { "colors": ["000000", "eef6ff", "..."],   // #rrggbb, 4 or 16 entries
          "romOffset": null,    // ROM offset where the 32 (or 8) bytes of the palette exist verbatim, or null
          "count": 40 }         // how many tile uses were seen
      ]
    }
  ],
  "rawTiles": [                 // uncompressed graphics: runs of tiles found as they are in the ROM
    { "offset": 1024, "length": 2048, "bpp": 4 }
  ],
  "rawPalettes": [ ]            // palettes seen with the raw tiles, same shape as above
}
```

## Music

A game whose music was worked out has a `"music"` section (`tools/rom-trace/spc_recipe.py`):

```json
"music": {
  "entry": 1280, "slot": 4960,         // where the engine starts; the address the song banks are loaded to
  "engine": [ {"offset": 458756, "length": 3646, "dest": 1280}, ... ],   // ROM blocks uploaded once to the sound chip
  "patch": [ {"dest": 16, "hex": "..."} ],   // the variables of the engine once it started (below its code)
  "dsp": "...", "io": "...", "cpu": {"pc": 1358, "a": 0, "x": 3, "y": 0, "psw": 66, "sp": 207},
  "banks": [ { "blocks": [ {"offset": 465077, "length": 5661, "dest": 4960} ],
               "songs": [ [0, 0, 1, 0], ... ] } ]   // values of the CPU ports that start a song
}
```

The driver builds one `.spc` file (the format of SPC players: the 64 KB of the chip, its registers and a tag) per song:
the blocks are copied from the ROM, the patch gives the engine its variables, and the song number is in the ports.
Nothing of the music is in the table: the blocks stay in the ROM. A "song" is a command that made sound while the
tool tried them, so it can be a sound effect or a jingle; the names are not known.

Coverage is the part of the on-screen tiles seen during the traced play, not the whole game. A missing palette
`romOffset` means the game builds that palette at run time (fades, mixes, decompression), not that it is absent.
