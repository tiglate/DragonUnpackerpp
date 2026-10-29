// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/TgaDecoder.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>

namespace Dup::Core
{
struct DdsResult
{
    std::optional<DecodedImage> Image;
    std::string Message; // why there is no image
};

/// First mip level of a DDS file, as RGBA, rows from the top of the file's data. Block compressed formats BC1 (DXT1),
/// BC2 (DXT3), BC3 (DXT5), BC4, BC5, BC6H (clipped to 8 bits) and BC7 (FourCC or DX10 header) and uncompressed 32 bits; other formats give a
/// message. The pixels are not flipped: the files of Unity textures store the rows bottom first (see drv_unity_export.pas),
/// the caller flips them when it knows.
[[nodiscard]] auto DecodeDds(std::span<const std::byte> data) -> DdsResult;

/// Flips the rows of an image.
void FlipRows(DecodedImage &image);
} // namespace Dup::Core
