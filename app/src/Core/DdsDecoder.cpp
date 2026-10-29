// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/DdsDecoder.h"

#include "Core/Bc6hTables.h"
#include "Core/Bc7Tables.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace Dup::Core
{
namespace
{
constexpr std::uint32_t MAX_SIDE = 32768;

auto Le32(const std::uint8_t *p) -> std::uint32_t
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

auto Le16(const std::uint8_t *p) -> unsigned
{
    return static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8);
}

using Rgba = std::array<std::uint8_t, 4>;

auto From565(unsigned value) -> Rgba
{
    const unsigned r = (value >> 11) & 31;
    const unsigned g = (value >> 5) & 63;
    const unsigned b = value & 31;
    return {static_cast<std::uint8_t>((r << 3) | (r >> 2)), static_cast<std::uint8_t>((g << 2) | (g >> 4)),
            static_cast<std::uint8_t>((b << 3) | (b >> 2)), 255};
}

/// The colour part of BC1/BC2/BC3: 16 pixels as RGBA; fourColour is false for the 3 colour + transparent mode of BC1.
void ColourBlock(const std::uint8_t *block, bool allowTransparent, std::array<Rgba, 16> &pixels)
{
    const unsigned c0 = Le16(block);
    const unsigned c1 = Le16(block + 2);
    std::array<Rgba, 4> palette{};
    palette[0] = From565(c0);
    palette[1] = From565(c1);
    if (c0 > c1 || !allowTransparent)
    {
        for (int i = 0; i < 3; ++i)
        {
            palette[2][i] = static_cast<std::uint8_t>((2 * palette[0][i] + palette[1][i]) / 3);
            palette[3][i] = static_cast<std::uint8_t>((palette[0][i] + 2 * palette[1][i]) / 3);
        }
        palette[2][3] = palette[3][3] = 255;
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            palette[2][i] = static_cast<std::uint8_t>((palette[0][i] + palette[1][i]) / 2);
        }
        palette[2][3] = 255;
        palette[3] = {0, 0, 0, 0};
    }
    const std::uint32_t bits = Le32(block + 4);
    for (int i = 0; i < 16; ++i)
    {
        pixels[static_cast<std::size_t>(i)] = palette[(bits >> (2 * i)) & 3];
    }
}

/// BC3 alpha / BC4 channel: 8 bytes, 16 values.
void AlphaBlock(const std::uint8_t *block, std::array<std::uint8_t, 16> &values)
{
    std::array<unsigned, 8> palette{};
    palette[0] = block[0];
    palette[1] = block[1];
    if (palette[0] > palette[1])
    {
        for (unsigned i = 1; i <= 6; ++i)
        {
            palette[i + 1] = ((7 - i) * palette[0] + i * palette[1]) / 7;
        }
    }
    else
    {
        for (unsigned i = 1; i <= 4; ++i)
        {
            palette[i + 1] = ((5 - i) * palette[0] + i * palette[1]) / 5;
        }
        palette[6] = 0;
        palette[7] = 255;
    }
    std::uint64_t bits = 0;
    for (int i = 0; i < 6; ++i)
    {
        bits |= static_cast<std::uint64_t>(block[2 + i]) << (8 * i);
    }
    for (int i = 0; i < 16; ++i)
    {
        values[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(palette[(bits >> (3 * i)) & 7]);
    }
}

/// Reads bits least significant first from a 16 byte block.
class BitReader
{
  public:
    explicit BitReader(const std::uint8_t *block) : _block(block)
    {
    }
    auto Read(int count) -> unsigned
    {
        unsigned value = 0;
        for (int i = 0; i < count; ++i, ++_position)
        {
            value |= static_cast<unsigned>((_block[_position >> 3] >> (_position & 7)) & 1) << i;
        }
        return value;
    }

  private:
    const std::uint8_t *_block;
    int _position{0};
};

struct Bc7Mode
{
    int Subsets;
    int PartitionBits;
    int RotationBits;
    int IndexSelectionBits;
    int ColourBits;
    int AlphaBits;
    int EndpointPBits; // one P bit per endpoint
    int SharedPBits;   // one P bit per subset
    int IndexBits[2];
};

constexpr std::array<Bc7Mode, 8> BC7_MODES{{{3, 4, 0, 0, 4, 0, 1, 0, {3, 0}},
                                            {2, 6, 0, 0, 6, 0, 0, 1, {3, 0}},
                                            {3, 6, 0, 0, 5, 0, 0, 0, {2, 0}},
                                            {2, 6, 0, 0, 7, 0, 1, 0, {2, 0}},
                                            {1, 0, 2, 1, 5, 6, 0, 0, {2, 3}},
                                            {1, 0, 2, 0, 7, 8, 0, 0, {2, 2}},
                                            {1, 0, 0, 0, 7, 7, 1, 0, {4, 0}},
                                            {2, 6, 0, 0, 5, 5, 1, 0, {2, 0}}}};

auto InterpolationWeight(int bits, unsigned index) -> unsigned
{
    static constexpr std::array<std::uint8_t, 4> W2{0, 21, 43, 64};
    static constexpr std::array<std::uint8_t, 8> W3{0, 9, 18, 27, 37, 46, 55, 64};
    static constexpr std::array<std::uint8_t, 16> W4{0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};
    return bits == 2 ? W2[index] : bits == 3 ? W3[index] : W4[index];
}

auto Expand(unsigned value, int bits) -> unsigned
{
    value <<= (8 - bits);
    return value | (value >> bits);
}

/// One BC7 block (16 bytes) as 16 RGBA pixels; a reserved mode (first byte 0) gives transparent black as D3D specifies.
void Bc7Block(const std::uint8_t *block, std::array<Rgba, 16> &pixels)
{
    int mode = 0;
    while (mode < 8 && ((block[0] >> mode) & 1) == 0)
    {
        ++mode;
    }
    if (mode == 8)
    {
        pixels.fill(Rgba{0, 0, 0, 0});
        return;
    }
    const Bc7Mode &info = BC7_MODES[static_cast<std::size_t>(mode)];
    BitReader reader(block);
    reader.Read(mode + 1);
    const unsigned partition = reader.Read(info.PartitionBits);
    const unsigned rotation = reader.Read(info.RotationBits);
    const unsigned indexSelection = reader.Read(info.IndexSelectionBits);

    const int endpoints = info.Subsets * 2;
    std::array<std::array<unsigned, 4>, 6> value{}; // endpoint, channel (r, g, b, a)
    for (int channel = 0; channel < 3; ++channel)
    {
        for (int e = 0; e < endpoints; ++e)
        {
            value[static_cast<std::size_t>(e)][static_cast<std::size_t>(channel)] = reader.Read(info.ColourBits);
        }
    }
    if (info.AlphaBits > 0)
    {
        for (int e = 0; e < endpoints; ++e)
        {
            value[static_cast<std::size_t>(e)][3] = reader.Read(info.AlphaBits);
        }
    }
    std::array<unsigned, 6> pBit{};
    if (info.EndpointPBits > 0)
    {
        for (int e = 0; e < endpoints; ++e)
        {
            pBit[static_cast<std::size_t>(e)] = reader.Read(1);
        }
    }
    else if (info.SharedPBits > 0)
    {
        for (int s = 0; s < info.Subsets; ++s)
        {
            const unsigned bit = reader.Read(1);
            pBit[static_cast<std::size_t>(s * 2)] = pBit[static_cast<std::size_t>(s * 2 + 1)] = bit;
        }
    }
    const bool hasP = info.EndpointPBits > 0 || info.SharedPBits > 0;
    std::array<std::array<unsigned, 4>, 6> endpoint{};
    for (int e = 0; e < endpoints; ++e)
    {
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            const bool isAlpha = channel == 3;
            const int bits = isAlpha ? info.AlphaBits : info.ColourBits;
            if (isAlpha && info.AlphaBits == 0)
            {
                endpoint[static_cast<std::size_t>(e)][channel] = 255;
                continue;
            }
            unsigned v = value[static_cast<std::size_t>(e)][channel];
            int total = bits;
            if (hasP)
            {
                v = (v << 1) | pBit[static_cast<std::size_t>(e)];
                total = bits + 1;
            }
            endpoint[static_cast<std::size_t>(e)][channel] = Expand(v, total);
        }
    }

    auto subsetOf = [&](int pixel) -> int {
        if (info.Subsets == 2)
        {
            return (Bc7Tables::PARTITIONS_2[partition] >> pixel) & 1;
        }
        if (info.Subsets == 3)
        {
            return static_cast<int>((Bc7Tables::PARTITIONS_3[partition] >> (2 * pixel)) & 3);
        }
        return 0;
    };
    auto isAnchor = [&](int pixel) -> bool {
        if (pixel == 0)
        {
            return true;
        }
        const int subset = subsetOf(pixel);
        if (info.Subsets == 2)
        {
            return subset == 1 && pixel == Bc7Tables::ANCHOR_2[partition];
        }
        if (info.Subsets == 3)
        {
            return subset > 0 && pixel == Bc7Tables::ANCHOR_3[static_cast<std::size_t>(subset - 1)][partition];
        }
        return false;
    };

    std::array<unsigned, 16> colourIndex{};
    std::array<unsigned, 16> alphaIndex{};
    const bool twoIndexSets = mode == 4 || mode == 5;
    for (int set = 0; set < (twoIndexSets ? 2 : 1); ++set)
    {
        const int bits = info.IndexBits[set];
        for (int pixel = 0; pixel < 16; ++pixel)
        {
            const unsigned index = reader.Read(isAnchor(pixel) ? bits - 1 : bits);
            (set == 0 ? colourIndex : alphaIndex)[static_cast<std::size_t>(pixel)] = index;
        }
    }
    int colourBits = info.IndexBits[0];
    int alphaBits = twoIndexSets ? info.IndexBits[1] : info.IndexBits[0];
    if (!twoIndexSets)
    {
        alphaIndex = colourIndex;
    }
    else if (mode == 4 && indexSelection == 1)
    {
        std::swap(colourIndex, alphaIndex);
        std::swap(colourBits, alphaBits);
    }

    for (int pixel = 0; pixel < 16; ++pixel)
    {
        const std::size_t e0 = static_cast<std::size_t>(subsetOf(pixel) * 2);
        const unsigned cw = InterpolationWeight(colourBits, colourIndex[static_cast<std::size_t>(pixel)]);
        const unsigned aw = InterpolationWeight(alphaBits, alphaIndex[static_cast<std::size_t>(pixel)]);
        Rgba out{};
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            const unsigned w = channel == 3 ? aw : cw;
            out[channel] = static_cast<std::uint8_t>((endpoint[e0][channel] * (64 - w) + endpoint[e0 + 1][channel] * w + 32) >> 6);
        }
        if (rotation != 0)
        {
            std::swap(out[3], out[rotation - 1]);
        }
        pixels[static_cast<std::size_t>(pixel)] = out;
    }
}

auto SignExtend(std::uint16_t value, unsigned bits) -> std::uint16_t
{
    const auto mask = static_cast<std::uint16_t>(1U << (bits - 1));
    return static_cast<std::uint16_t>((value ^ mask) - mask);
}

/// Endpoint value (bits wide) to the 16 bit scale of the half floats.
auto UnquantizeEndpoint(std::uint16_t value, bool isSigned, unsigned bits) -> std::uint16_t
{
    const auto maximum = static_cast<std::uint16_t>(1U << (bits - 1));
    if (isSigned)
    {
        if (bits >= 16)
        {
            return value;
        }
        const bool negative = (value & 0x8000) != 0;
        value &= 0x7fff;
        std::uint16_t magnitude;
        if (value == 0)
        {
            magnitude = 0;
        }
        else if (value >= maximum - 1)
        {
            magnitude = 0x7fff;
        }
        else
        {
            magnitude = static_cast<std::uint16_t>(((value << 15) + 0x4000) >> (bits - 1));
        }
        return negative ? static_cast<std::uint16_t>(-magnitude) : magnitude;
    }
    if (bits >= 15)
    {
        return value;
    }
    if (value == 0)
    {
        return 0;
    }
    if (value == maximum)
    {
        return 0xFFFF;
    }
    return static_cast<std::uint16_t>(((value << 15) + 0x4000) >> (bits - 1));
}

auto FinishUnquantize(unsigned value, bool isSigned) -> std::uint16_t
{
    if (isSigned)
    {
        const unsigned sign = value & 0x8000;
        value &= 0x7fff;
        return static_cast<std::uint16_t>(((value * 31) >> 5) | sign);
    }
    return static_cast<std::uint16_t>((value * 31) >> 6);
}

auto HalfToByte(std::uint16_t half) -> std::uint8_t
{
    const int exponent = (half >> 10) & 31;
    const int mantissa = half & 1023;
    float value;
    if (exponent == 0)
    {
        value = std::ldexp(static_cast<float>(mantissa), -24);
    }
    else if (exponent == 31)
    {
        value = mantissa == 0 ? 65504.0F : 0.0F; // infinity: the largest value; not a number: 0
    }
    else
    {
        value = std::ldexp(static_cast<float>(mantissa + 1024), exponent - 25);
    }
    if ((half & 0x8000) != 0)
    {
        value = -value;
    }
    return static_cast<std::uint8_t>(std::clamp(std::round(value * 255.0F), 0.0F, 255.0F)); // HDR values are clipped to the display range
}

/// One BC6H block (16 bytes): unsigned (BC6H_UF16) or signed (BC6H_SF16) half floats, clipped to 8 bits per channel.
void Bc6hBlock(const std::uint8_t *block, bool isSigned, std::array<Rgba, 16> &pixels)
{
    using namespace Bc6hTables;
    BitReader reader(block);
    unsigned mode = reader.Read(2);
    if ((mode & 2) != 0)
    {
        mode |= reader.Read(3) << 2;
    }
    const ModeInfo &info = MODES[mode];
    if (info.EndpointBits == 0)
    {
        pixels.fill(Rgba{0, 0, 0, 255}); // a reserved mode
        return;
    }
    std::array<std::array<std::uint16_t, 4>, 3> endpoint{}; // channel, end point
    for (std::size_t f = 0; f < FIELD_COUNTS[mode]; ++f)
    {
        const Field &field = FIELDS[mode][f];
        endpoint[field.Channel][field.Endpoint] = static_cast<std::uint16_t>(endpoint[field.Channel][field.Endpoint] | (reader.Read(field.Count) << field.Shift));
    }
    if (isSigned)
    {
        for (auto &channel : endpoint)
        {
            channel[0] = SignExtend(channel[0], info.EndpointBits);
        }
    }
    const unsigned subsets = info.PartitionBits != 0 ? 2 : 1;
    for (unsigned e = 1; e < subsets * 2; ++e)
    {
        for (std::size_t c = 0; c < 3; ++c)
        {
            if (isSigned || info.Transformed != 0)
            {
                endpoint[c][e] = SignExtend(endpoint[c][e], info.DeltaBits[c]);
            }
            if (info.Transformed != 0)
            {
                const auto mask = static_cast<std::uint16_t>((1U << info.EndpointBits) - 1);
                endpoint[c][e] = static_cast<std::uint16_t>((endpoint[c][e] + endpoint[c][0]) & mask);
                if (isSigned)
                {
                    endpoint[c][e] = SignExtend(endpoint[c][e], info.EndpointBits);
                }
            }
        }
    }
    for (unsigned e = 0; e < subsets * 2; ++e)
    {
        for (std::size_t c = 0; c < 3; ++c)
        {
            endpoint[c][e] = UnquantizeEndpoint(endpoint[c][e], isSigned, info.EndpointBits);
        }
    }
    const unsigned partition = info.PartitionBits != 0 ? reader.Read(5) : 0;
    const int indexBits = info.PartitionBits != 0 ? 3 : 4;
    for (int pixel = 0; pixel < 16; ++pixel)
    {
        unsigned subset = 0;
        int anchor = 0;
        if (info.PartitionBits != 0)
        {
            subset = (Bc7Tables::PARTITIONS_2[partition] >> pixel) & 1;
            anchor = subset != 0 ? Bc7Tables::ANCHOR_2[partition] : 0;
        }
        const unsigned index = reader.Read(pixel == anchor ? indexBits - 1 : indexBits);
        const unsigned weight = InterpolationWeight(indexBits, index);
        Rgba out{};
        for (std::size_t c = 0; c < 3; ++c)
        {
            const unsigned mixed = (endpoint[c][subset * 2] * (64 - weight) + endpoint[c][subset * 2 + 1] * weight + 32) >> 6;
            out[c] = HalfToByte(FinishUnquantize(mixed, isSigned));
        }
        out[3] = 255;
        pixels[static_cast<std::size_t>(pixel)] = out;
    }
}

enum class Kind
{
    BC1,
    BC2,
    BC3,
    BC4,
    BC5,
    BC7,
    BC6H_UNSIGNED,
    BC6H_SIGNED,
    RGBA32,
    BGRA32,
    UNSUPPORTED
};

auto FromDxgi(std::uint32_t format) -> Kind
{
    switch (format)
    {
    case 71:
    case 72:
        return Kind::BC1;
    case 74:
    case 75:
        return Kind::BC2;
    case 77:
    case 78:
        return Kind::BC3;
    case 80:
    case 81:
        return Kind::BC4;
    case 83:
    case 84:
        return Kind::BC5;
    case 97:
    case 98:
    case 99:
        return Kind::BC7;
    case 94:
    case 95:
        return Kind::BC6H_UNSIGNED;
    case 96:
        return Kind::BC6H_SIGNED;
    case 28:
    case 29:
        return Kind::RGBA32;
    case 87:
    case 91:
        return Kind::BGRA32;
    default:
        return Kind::UNSUPPORTED;
    }
}
} // namespace

auto DecodeDds(std::span<const std::byte> data) -> DdsResult
{
    DdsResult result;
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(data.data());
    if (data.size() < 128 || std::memcmp(bytes, "DDS ", 4) != 0 || Le32(bytes + 4) != 124)
    {
        result.Message = "not a DDS file";
        return result;
    }
    const std::uint32_t height = Le32(bytes + 12);
    const std::uint32_t width = Le32(bytes + 16);
    const std::uint32_t pixelFlags = Le32(bytes + 80);
    const std::uint8_t *fourCc = bytes + 84;
    std::size_t offset = 128;
    Kind kind = Kind::UNSUPPORTED;
    if ((pixelFlags & 4) != 0)
    {
        if (std::memcmp(fourCc, "DXT1", 4) == 0)
        {
            kind = Kind::BC1;
        }
        else if (std::memcmp(fourCc, "DXT3", 4) == 0)
        {
            kind = Kind::BC2;
        }
        else if (std::memcmp(fourCc, "DXT5", 4) == 0)
        {
            kind = Kind::BC3;
        }
        else if (std::memcmp(fourCc, "BC4U", 4) == 0 || std::memcmp(fourCc, "ATI1", 4) == 0)
        {
            kind = Kind::BC4;
        }
        else if (std::memcmp(fourCc, "BC5U", 4) == 0 || std::memcmp(fourCc, "ATI2", 4) == 0)
        {
            kind = Kind::BC5;
        }
        else if (std::memcmp(fourCc, "DX10", 4) == 0)
        {
            if (data.size() < 148)
            {
                result.Message = "truncated DX10 header";
                return result;
            }
            kind = FromDxgi(Le32(bytes + 128));
            if (kind == Kind::UNSUPPORTED)
            {
                result.Message = "the pixel format (DXGI " + std::to_string(Le32(bytes + 128)) + ") is not supported by the preview yet";
                return result;
            }
            offset = 148;
        }
    }
    else if ((pixelFlags & 0x41) == 0x41 && Le32(bytes + 88) == 32)
    {
        kind = Le32(bytes + 92) == 0xFF ? Kind::RGBA32 : Kind::BGRA32; // red mask
    }
    if (kind == Kind::UNSUPPORTED)
    {
        result.Message = "unsupported DDS pixel format";
        return result;
    }
    if (width == 0 || height == 0 || width > MAX_SIDE || height > MAX_SIDE)
    {
        result.Message = "invalid size";
        return result;
    }

    DecodedImage image;
    image.Width = width;
    image.Height = height;
    image.Rgba.assign(static_cast<std::size_t>(width) * height * 4, 255);
    const std::uint8_t *source = bytes + offset;
    const std::size_t available = data.size() - offset;

    if (kind == Kind::RGBA32 || kind == Kind::BGRA32)
    {
        const std::size_t need = static_cast<std::size_t>(width) * height * 4;
        if (available < need)
        {
            result.Message = "truncated data";
            return result;
        }
        for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i)
        {
            const std::uint8_t *p = source + i * 4;
            image.Rgba[i * 4 + 0] = kind == Kind::RGBA32 ? p[0] : p[2];
            image.Rgba[i * 4 + 1] = p[1];
            image.Rgba[i * 4 + 2] = kind == Kind::RGBA32 ? p[2] : p[0];
            image.Rgba[i * 4 + 3] = p[3];
        }
        result.Image = std::move(image);
        return result;
    }

    const std::size_t blockBytes = (kind == Kind::BC1 || kind == Kind::BC4) ? 8 : 16;
    const std::uint32_t blocksX = (width + 3) / 4;
    const std::uint32_t blocksY = (height + 3) / 4;
    if (available < static_cast<std::size_t>(blocksX) * blocksY * blockBytes)
    {
        result.Message = "truncated data";
        return result;
    }
    for (std::uint32_t by = 0; by < blocksY; ++by)
    {
        for (std::uint32_t bx = 0; bx < blocksX; ++bx)
        {
            const std::uint8_t *block = source + (static_cast<std::size_t>(by) * blocksX + bx) * blockBytes;
            std::array<Rgba, 16> pixels{};
            switch (kind)
            {
            case Kind::BC1:
                ColourBlock(block, true, pixels);
                break;
            case Kind::BC2: {
                ColourBlock(block + 8, false, pixels);
                for (int i = 0; i < 16; ++i)
                {
                    const unsigned nibble = (block[i / 2] >> (4 * (i & 1))) & 15;
                    pixels[static_cast<std::size_t>(i)][3] = static_cast<std::uint8_t>(nibble * 17);
                }
                break;
            }
            case Kind::BC3: {
                ColourBlock(block + 8, false, pixels);
                std::array<std::uint8_t, 16> alpha{};
                AlphaBlock(block, alpha);
                for (std::size_t i = 0; i < 16; ++i)
                {
                    pixels[i][3] = alpha[i];
                }
                break;
            }
            case Kind::BC4: {
                std::array<std::uint8_t, 16> red{};
                AlphaBlock(block, red);
                for (std::size_t i = 0; i < 16; ++i)
                {
                    pixels[i] = {red[i], red[i], red[i], 255};
                }
                break;
            }
            case Kind::BC7:
                Bc7Block(block, pixels);
                break;
            case Kind::BC6H_UNSIGNED:
            case Kind::BC6H_SIGNED:
                Bc6hBlock(block, kind == Kind::BC6H_SIGNED, pixels);
                break;
            default: { // BC5: two channels (usually a normal map), the blue one is rebuilt
                std::array<std::uint8_t, 16> red{};
                std::array<std::uint8_t, 16> green{};
                AlphaBlock(block, red);
                AlphaBlock(block + 8, green);
                for (std::size_t i = 0; i < 16; ++i)
                {
                    const double x = red[i] / 127.5 - 1.0;
                    const double y = green[i] / 127.5 - 1.0;
                    const double z2 = std::max(0.0, 1.0 - x * x - y * y);
                    pixels[i] = {red[i], green[i], static_cast<std::uint8_t>(std::sqrt(z2) * 127.5 + 127.5), 255};
                }
                break;
            }
            }
            for (std::uint32_t y = 0; y < 4 && by * 4 + y < height; ++y)
            {
                for (std::uint32_t x = 0; x < 4 && bx * 4 + x < width; ++x)
                {
                    const auto &p = pixels[y * 4 + x];
                    std::memcpy(&image.Rgba[((static_cast<std::size_t>(by) * 4 + y) * width + bx * 4 + x) * 4], p.data(), 4);
                }
            }
        }
    }
    result.Image = std::move(image);
    return result;
}

void FlipRows(DecodedImage &image)
{
    const std::size_t stride = static_cast<std::size_t>(image.Width) * 4;
    for (std::size_t top = 0, bottom = image.Height; top + 1 < bottom; ++top)
    {
        --bottom;
        std::swap_ranges(image.Rgba.begin() + static_cast<std::ptrdiff_t>(top * stride), image.Rgba.begin() + static_cast<std::ptrdiff_t>((top + 1) * stride),
                         image.Rgba.begin() + static_cast<std::ptrdiff_t>(bottom * stride));
    }
}
} // namespace Dup::Core
