// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/PngEncoder.h"

#include <zlib.h>

#include <cstdint>
#include <cstring>

namespace Dup::Core
{
namespace
{
void AppendBe32(std::vector<std::byte> &out, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        out.push_back(static_cast<std::byte>((value >> shift) & 0xFF));
    }
}

void AppendChunk(std::vector<std::byte> &out, const char *type, const std::vector<std::uint8_t> &data)
{
    AppendBe32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t crcStart = out.size();
    for (int i = 0; i < 4; ++i)
    {
        out.push_back(static_cast<std::byte>(type[i]));
    }
    for (const auto byte : data)
    {
        out.push_back(static_cast<std::byte>(byte));
    }
    const auto crc = static_cast<std::uint32_t>(crc32(0L, reinterpret_cast<const Bytef *>(out.data() + crcStart), static_cast<uInt>(out.size() - crcStart)));
    AppendBe32(out, crc);
}
} // namespace

auto EncodePng(const DecodedImage &image) -> std::optional<std::vector<std::byte>>
{
    if (image.Width == 0 || image.Height == 0 || image.Width > 65535 || image.Height > 65535 || image.Rgba.size() != static_cast<std::size_t>(image.Width) * image.Height * 4)
    {
        return std::nullopt;
    }
    // each scanline starts with its filter type (0: none)
    const std::size_t stride = static_cast<std::size_t>(image.Width) * 4;
    std::vector<std::uint8_t> raw((stride + 1) * image.Height);
    for (std::size_t y = 0; y < image.Height; ++y)
    {
        raw[y * (stride + 1)] = 0;
        std::memcpy(&raw[y * (stride + 1) + 1], &image.Rgba[y * stride], stride);
    }
    uLongf packedSize = compressBound(static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> packed(packedSize);
    if (compress2(packed.data(), &packedSize, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK)
    {
        return std::nullopt;
    }
    packed.resize(packedSize);

    std::vector<std::byte> out;
    for (const int b : std::initializer_list<int>{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A})
    {
        out.push_back(static_cast<std::byte>(b));
    }
    std::vector<std::uint8_t> header;
    for (const std::uint32_t v : {image.Width, image.Height})
    {
        for (int shift = 24; shift >= 0; shift -= 8)
        {
            header.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFF));
        }
    }
    header.insert(header.end(), {8, 6, 0, 0, 0}); // 8 bits, RGBA, deflate, no filter method change, no interlace
    AppendChunk(out, "IHDR", header);
    AppendChunk(out, "IDAT", packed);
    AppendChunk(out, "IEND", {});
    return out;
}
} // namespace Dup::Core
