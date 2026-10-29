// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/TgaDecoder.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace Dup::Core
{
/// A PNG file (8 bits RGBA, no filtering, deflate by zlib) of the image; nullopt when the image is empty or too large.
[[nodiscard]] auto EncodePng(const DecodedImage &image) -> std::optional<std::vector<std::byte>>;
} // namespace Dup::Core
