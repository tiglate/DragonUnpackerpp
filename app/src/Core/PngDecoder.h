// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/TgaDecoder.h"

#include <cstddef>
#include <optional>
#include <span>

namespace Dup::Core
{
/// A PNG of 8 bits gray, RGB or RGBA (with or without alpha), not interlaced, any of the five filters: what the drivers write
/// (the lightmaps of the Requiem levels in their glTF). nullopt for any other kind or a damaged file.
[[nodiscard]] auto DecodePng(std::span<const std::byte> data) -> std::optional<DecodedImage>;
} // namespace Dup::Core
