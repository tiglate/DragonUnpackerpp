// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/TgaDecoder.h"

#include <algorithm>
#include <array>

namespace Dup::Core
{
namespace
{
constexpr std::size_t HEADER_SIZE = 18;
constexpr std::uint32_t MAX_SIDE = 32768;

struct Cursor
{
    std::span<const std::byte> Data;
    std::size_t Position{0};

    [[nodiscard]] auto Left() const -> std::size_t
    {
        return Data.size() - Position;
    }
    auto Take(std::size_t count) -> const std::uint8_t *
    {
        const auto *result = reinterpret_cast<const std::uint8_t *>(Data.data()) + Position;
        Position += count;
        return result;
    }
};

auto Expand5(unsigned value) -> std::uint8_t
{
    return static_cast<std::uint8_t>((value << 3) | (value >> 2));
}

/// One source pixel (bytes per pixel = depth / 8, rounded up) to RGBA.
void ToRgba(const std::uint8_t *source, unsigned depth, std::uint8_t *target)
{
    switch (depth)
    {
    case 8:
        target[0] = target[1] = target[2] = source[0];
        target[3] = 255;
        break;
    case 15:
    case 16: {
        const unsigned value = source[0] | (source[1] << 8);
        target[0] = Expand5((value >> 10) & 31);
        target[1] = Expand5((value >> 5) & 31);
        target[2] = Expand5(value & 31);
        target[3] = (depth == 16 && (value & 0x8000) == 0) ? 0 : 255;
        break;
    }
    case 24:
        target[0] = source[2];
        target[1] = source[1];
        target[2] = source[0];
        target[3] = 255;
        break;
    default: // 32
        target[0] = source[2];
        target[1] = source[1];
        target[2] = source[0];
        target[3] = source[3];
        break;
    }
}
} // namespace

auto DecodeTga(std::span<const std::byte> data) -> std::optional<DecodedImage>
{
    if (data.size() < HEADER_SIZE)
    {
        return std::nullopt;
    }
    Cursor cursor{data, 0};
    const std::uint8_t *header = cursor.Take(HEADER_SIZE);
    const unsigned idLength = header[0];
    const unsigned colourMapType = header[1];
    const unsigned imageType = header[2];
    const unsigned colourMapLength = header[5] | (header[6] << 8);
    const unsigned colourMapDepth = header[7];
    const std::uint32_t width = header[12] | (header[13] << 8);
    const std::uint32_t height = header[14] | (header[15] << 8);
    const unsigned depth = header[16];
    const unsigned descriptor = header[17];

    const bool rle = imageType >= 9 && imageType <= 11;
    const unsigned baseType = rle ? imageType - 8 : imageType;
    if (colourMapType != 0 || (baseType != 2 && baseType != 3) || width == 0 || height == 0 || width > MAX_SIDE || height > MAX_SIDE)
    {
        return std::nullopt; // colour mapped images are not produced by the drivers
    }
    if ((baseType == 3 && depth != 8) || (baseType == 2 && depth != 15 && depth != 16 && depth != 24 && depth != 32))
    {
        return std::nullopt;
    }
    const std::size_t skip = static_cast<std::size_t>(idLength) + static_cast<std::size_t>(colourMapLength) * ((colourMapDepth + 7) / 8);
    if (cursor.Left() < skip)
    {
        return std::nullopt;
    }
    cursor.Take(skip);

    const std::size_t bytesPerPixel = (depth + 7) / 8;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    DecodedImage image;
    image.Width = width;
    image.Height = height;
    image.Rgba.resize(pixelCount * 4);

    const bool topFirst = (descriptor & 0x20) != 0;
    const bool rightFirst = (descriptor & 0x10) != 0;
    auto place = [&](std::size_t index, const std::uint8_t *source) {
        const std::size_t row = index / width;
        const std::size_t column = index % width;
        const std::size_t y = topFirst ? row : height - 1 - row;
        const std::size_t x = rightFirst ? width - 1 - column : column;
        ToRgba(source, depth, &image.Rgba[(y * width + x) * 4]);
    };

    std::size_t index = 0;
    if (!rle)
    {
        if (cursor.Left() < pixelCount * bytesPerPixel)
        {
            return std::nullopt;
        }
        for (; index < pixelCount; ++index)
        {
            place(index, cursor.Take(bytesPerPixel));
        }
    }
    else
    {
        while (index < pixelCount)
        {
            if (cursor.Left() < 1)
            {
                return std::nullopt;
            }
            const unsigned packet = *cursor.Take(1);
            const std::size_t count = std::min<std::size_t>((packet & 127) + 1, pixelCount - index);
            if ((packet & 128) != 0)
            {
                if (cursor.Left() < bytesPerPixel)
                {
                    return std::nullopt;
                }
                const std::uint8_t *source = cursor.Take(bytesPerPixel);
                for (std::size_t k = 0; k < count; ++k, ++index)
                {
                    place(index, source);
                }
            }
            else
            {
                if (cursor.Left() < count * bytesPerPixel)
                {
                    return std::nullopt;
                }
                for (std::size_t k = 0; k < count; ++k, ++index)
                {
                    place(index, cursor.Take(bytesPerPixel));
                }
            }
        }
    }
    return image;
}
} // namespace Dup::Core
