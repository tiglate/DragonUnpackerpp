// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/TgaDecoder.h"
#include "Dudi7/IArchive.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Dup::Core
{
/// 256 colours as red, green, blue.
using PaletteRgb = std::array<std::uint8_t, 768>;

/// The picture and texture formats of the "pictex" converter of v5 (cnv_pictex), decoded to pixels: Quake and Half-Life WAD
/// textures and pictures, BloodRayne POD3 and POD5 textures, Monkey Island Special Edition DXT, Glacier (Hitman) RGBA, PALN
/// and DXT textures, Build ART tiles. They are recognized by the format of the archive, the name of the entry and the two
/// values the driver keeps for it.
struct TextureEntry
{
    std::string FormatId;
    std::string Name;
    std::int64_t DataX{0};
    std::int64_t DataY{0};
};

/// Is this entry a texture of one of those formats?
[[nodiscard]] auto IsTextureEntry(const TextureEntry &entry) -> bool;

/// Does it need a palette that is not in its data (Quake pictures, console pictures, ART tiles)?
[[nodiscard]] auto NeedsPalette(const TextureEntry &entry) -> bool;

/// The pixels of the entry; nullopt if the data is not what it should be, or a palette is needed and none is given.
[[nodiscard]] auto DecodeTexture(const TextureEntry &entry, std::span<const std::byte> bytes, const PaletteRgb *palette) -> std::optional<DecodedImage>;

/// A palette file: 768 bytes (6 bits per component as in Build's PALETTE.DAT, or 8 bits, only the first palette of PLAYPAL),
/// or a JASC-PAL text file.
[[nodiscard]] auto ParsePalette(std::span<const std::byte> bytes) -> std::optional<PaletteRgb>;

/// Is it the name of an entry that holds the palette of the other entries (palette.dat, palette.lmp, playpal...)?
[[nodiscard]] auto IsPaletteEntryName(std::string_view name) -> bool;

/// The palette of an archive: its entry named for that, or nullopt.
[[nodiscard]] auto FindPaletteInArchive(const Dudi7::IArchive &archive) -> std::optional<PaletteRgb>;

/// Finds the palette of an archive once (for a thread that reads many entries); falls back to the palette of the options.
class PaletteLookup
{
  public:
    /// nullptr when neither the archive nor the fallback has one.
    [[nodiscard]] auto Get(const Dudi7::IArchive &archive, const std::optional<PaletteRgb> &fallback) -> const PaletteRgb *
    {
        if (!_searched)
        {
            _found = FindPaletteInArchive(archive);
            _searched = true;
        }
        return _found ? &*_found : (fallback ? &*fallback : nullptr);
    }
    void Reset()
    {
        _searched = false;
        _found.reset();
    }

  private:
    bool _searched{false};
    std::optional<PaletteRgb> _found;
};

/// Names whose extension may be one of a texture of those formats (to look for thumbnails before the format is known).
[[nodiscard]] auto HasTextureExtension(std::string_view fileName) -> bool;

/// A palette read from a file (the one chosen in the options).
[[nodiscard]] auto LoadPaletteFile(const std::string &path) -> std::optional<PaletteRgb>;
} // namespace Dup::Core
