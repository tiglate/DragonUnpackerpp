// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/TgaDecoder.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Dup::Core
{
/// An Outrage Graphics Format image (Descent 3 .ogf): the stored mip levels, largest first.
struct OgfImage
{
    std::string Name;
    std::uint8_t Type{0};                 // 121 to 124 or 127
    std::vector<DecodedImage> Levels;     // the stored levels only (the engine generates the rest)
    std::size_t Consumed{0};              // bytes used, from the start of the data given: the next OAF frame begins there
};

/// An Outrage Animation Format file (.oaf): a sequence of OGF images.
struct OafAnimation
{
    int Version{1};
    bool Legacy{false};   // the 17 bytes header without the 127 marker
    float FrameTime{0};   // as stored (the game plays 0.07 seconds per frame whatever it says)
    std::vector<OgfImage> Frames;
};

/// The fixed-header Outrage bitmap font (.fnt).
struct FntFont
{
    std::string Name;
    int Width{0};
    int Height{0};
    unsigned Flags{0};
    unsigned FirstChar{0};
    unsigned LastChar{0};
    int Tracking{0};
    std::vector<int> Widths;                         // one per character, FirstChar to LastChar
    std::vector<DecodedImage> Glyphs;                // same order; mono ink is white, the rest transparent
    struct Kerning { std::uint8_t First, Second; std::int8_t Adjust; };
    std::vector<Kerning> Kerns;
};

/// Does this start like an OGF (type byte 121-124 or 127 and a zero colour-map type)? Cheap check for the type identification.
[[nodiscard]] auto LooksLikeOgf(std::span<const std::byte> data) -> bool;

/// Decodes one OGF at the start of data. nullopt (with a reason in error when given) for anything malformed or of an
/// unsupported profile (types 125 and 126 have no pixel decoder in the game either).
[[nodiscard]] auto DecodeOgf(std::span<const std::byte> data, std::string *error = nullptr) -> std::optional<OgfImage>;

/// Decodes an OAF: the 7 bytes header (marker 127, version 1) or the legacy 17 bytes one.
[[nodiscard]] auto DecodeOaf(std::span<const std::byte> data, std::string *error = nullptr) -> std::optional<OafAnimation>;

[[nodiscard]] auto IsFnt(std::span<const std::byte> data) -> bool;
[[nodiscard]] auto DecodeFnt(std::span<const std::byte> data, std::string *error = nullptr) -> std::optional<FntFont>;

/// All the frames of an animation side by side (a square-ish grid), for the preview and the PNG conversion.
[[nodiscard]] auto MakeFrameSheet(const OafAnimation &animation) -> DecodedImage;

/// The glyphs of a font on a dark background, 16 to a row, for the preview and the PNG conversion.
[[nodiscard]] auto MakeGlyphSheet(const FntFont &font) -> DecodedImage;
} // namespace Dup::Core
