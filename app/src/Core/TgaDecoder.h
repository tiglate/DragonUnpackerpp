// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Dup::Core
{
/// Pixels as 8 bits R, G, B, A, rows from the top.
struct DecodedImage
{
    std::uint32_t Width{0};
    std::uint32_t Height{0};
    std::vector<std::uint8_t> Rgba;
};

/// Uncompressed and RLE TGA, 8 bits gray, 8 bits colour mapped (the map has 15/16/24/32 bits colours), 15/16/24/32 bits
/// colour, any origin. The files written by the drivers have no TGA 2.0 footer, which Qt's own plugin requires, hence this
/// decoder. Returns nullopt for anything it does not understand.
/// For a colour mapped image, palette (256 colours as red, green, blue) replaces the map when given: that is how the
/// tiles of a console ROM, written in grays, are shown with the colours of a palette found in the ROM.
[[nodiscard]] auto DecodeTga(std::span<const std::byte> data, const std::array<std::uint8_t, 768> *palette = nullptr) -> std::optional<DecodedImage>;
} // namespace Dup::Core
