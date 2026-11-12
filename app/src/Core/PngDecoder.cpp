// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/PngDecoder.h"

#include <zlib.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Dup::Core
{
namespace
{
constexpr std::uint32_t MAX_SIDE = 16384;

auto Be32(const std::byte *p) -> std::uint32_t
{
    return static_cast<std::uint32_t>(p[0]) << 24 | static_cast<std::uint32_t>(p[1]) << 16 | static_cast<std::uint32_t>(p[2]) << 8 |
           static_cast<std::uint32_t>(p[3]);
}

auto Paeth(int a, int b, int c) -> int
{
    const int p = a + b - c;
    const int pa = std::abs(p - a);
    const int pb = std::abs(p - b);
    const int pc = std::abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}
} // namespace

auto DecodePng(std::span<const std::byte> data) -> std::optional<DecodedImage>
{
    static const std::uint8_t SIGNATURE[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (data.size() < 8 || std::memcmp(data.data(), SIGNATURE, 8) != 0)
    {
        return std::nullopt;
    }
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t channels = 0;
    std::vector<std::uint8_t> packed;
    for (std::size_t at = 8; at + 12 <= data.size();)
    {
        const std::uint32_t length = Be32(&data[at]);
        if (length > data.size() - at - 12)
        {
            return std::nullopt;
        }
        const char *type = reinterpret_cast<const char *>(&data[at + 4]);
        const std::byte *body = &data[at + 8];
        if (std::memcmp(type, "IHDR", 4) == 0)
        {
            if (length < 13)
            {
                return std::nullopt;
            }
            width = Be32(body);
            height = Be32(body + 4);
            const auto depth = static_cast<int>(body[8]);
            const auto colour = static_cast<int>(body[9]);
            const auto interlace = static_cast<int>(body[12]);
            channels = colour == 0 ? 1 : colour == 2 ? 3 : colour == 4 ? 2 : colour == 6 ? 4 : 0;
            if (depth != 8 || channels == 0 || interlace != 0 || width == 0 || height == 0 || width > MAX_SIDE || height > MAX_SIDE)
            {
                return std::nullopt;
            }
        }
        else if (std::memcmp(type, "IDAT", 4) == 0)
        {
            packed.insert(packed.end(), reinterpret_cast<const std::uint8_t *>(body), reinterpret_cast<const std::uint8_t *>(body) + length);
        }
        else if (std::memcmp(type, "IEND", 4) == 0)
        {
            break;
        }
        at += 12 + static_cast<std::size_t>(length);
    }
    if (channels == 0 || packed.empty())
    {
        return std::nullopt;
    }
    const std::size_t stride = static_cast<std::size_t>(width) * channels;
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(height) * (stride + 1));
    uLongf rawSize = static_cast<uLongf>(raw.size());
    if (uncompress(raw.data(), &rawSize, packed.data(), static_cast<uLong>(packed.size())) != Z_OK || rawSize != raw.size())
    {
        return std::nullopt;
    }
    // undo the filters, row by row, in place (each row without its filter byte goes to `rows`)
    std::vector<std::uint8_t> rows(static_cast<std::size_t>(height) * stride);
    for (std::size_t y = 0; y < height; ++y)
    {
        const std::uint8_t filter = raw[y * (stride + 1)];
        const std::uint8_t *in = &raw[y * (stride + 1) + 1];
        std::uint8_t *out = &rows[y * stride];
        const std::uint8_t *up = y > 0 ? &rows[(y - 1) * stride] : nullptr;
        for (std::size_t x = 0; x < stride; ++x)
        {
            const int a = x >= channels ? out[x - channels] : 0;
            const int b = up != nullptr ? up[x] : 0;
            const int c = up != nullptr && x >= channels ? up[x - channels] : 0;
            int predicted = 0;
            switch (filter)
            {
            case 0: predicted = 0; break;
            case 1: predicted = a; break;
            case 2: predicted = b; break;
            case 3: predicted = (a + b) / 2; break;
            case 4: predicted = Paeth(a, b, c); break;
            default: return std::nullopt;
            }
            out[x] = static_cast<std::uint8_t>(in[x] + predicted);
        }
    }
    DecodedImage image;
    image.Width = width;
    image.Height = height;
    image.Rgba.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i)
    {
        const std::uint8_t *p = &rows[i * channels];
        std::uint8_t *q = &image.Rgba[i * 4];
        if (channels <= 2)
        {
            q[0] = q[1] = q[2] = p[0];
            q[3] = channels == 2 ? p[1] : 255;
        }
        else
        {
            q[0] = p[0];
            q[1] = p[1];
            q[2] = p[2];
            q[3] = channels == 4 ? p[3] : 255;
        }
    }
    return image;
}
} // namespace Dup::Core
