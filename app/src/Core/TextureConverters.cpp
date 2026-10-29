// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/TextureConverters.h"

#include "Core/DdsDecoder.h"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Dup::Core
{
namespace
{
enum class Kind
{
    NONE,
    WAD_PICTURE,   // QPIC: width, height, pixels (palette elsewhere)
    WAD3_MIPTEX,   // Half-Life: with its palette at the end
    WAD2_MIPTEX,   // Quake: palette elsewhere
    WAD_CONSOLE,   // 128x128 or 320x200 raw pixels
    POD3_TEX,
    POD5_TEX,
    KAPL_DXT,
    GTEX_RGBA,
    GTEX_PALN,
    GTEX_DXT,
    ART_TILE,
    SPEC_PICTURE,
    SPEC_CHARACTER,
    SPEC_PARTICLE
};

auto Lower(std::string_view text) -> std::string
{
    std::string result(text);
    for (auto &c : result)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

auto ExtensionOf(std::string_view name) -> std::string
{
    const auto dot = name.rfind('.');
    return dot == std::string_view::npos ? std::string() : Lower(name.substr(dot + 1));
}

auto KindOf(const TextureEntry &entry) -> Kind
{
    const std::string extension = ExtensionOf(entry.Name);
    if (entry.FormatId == "WAD2" || entry.FormatId == "WAD3")
    {
        if (extension == "psb")
        {
            return Kind::WAD_PICTURE;
        }
        if (extension == "pmm")
        {
            return Kind::WAD3_MIPTEX;
        }
        if (extension == "mm")
        {
            return entry.FormatId == "WAD2" && Lower(entry.Name).rfind("conchars", 0) == 0 ? Kind::WAD_CONSOLE : Kind::WAD2_MIPTEX;
        }
        if (extension == "cp")
        {
            return Kind::WAD_CONSOLE;
        }
        return Kind::NONE;
    }
    if (entry.FormatId == "POD3" && extension == "tex")
    {
        return Kind::POD3_TEX;
    }
    if (entry.FormatId == "POD5" && extension == "tex")
    {
        return Kind::POD5_TEX;
    }
    if (entry.FormatId == "KAPL" && extension == "dxt")
    {
        return Kind::KAPL_DXT;
    }
    if (entry.FormatId == "GTEX" || entry.FormatId == "HMCTEX")
    {
        if (extension == "rgba")
        {
            return Kind::GTEX_RGBA;
        }
        if (extension == "paln")
        {
            return Kind::GTEX_PALN;
        }
        if (extension == "dxt1" || extension == "dxt3")
        {
            return Kind::GTEX_DXT;
        }
        return Kind::NONE;
    }
    if ((entry.FormatId == "SPEC" || entry.FormatId == "DPA") && extension == "chr")
    {
        return Kind::SPEC_CHARACTER; // a sprite: the picture, then the hit damage, the center, the boundaries (index 0 is transparent)
    }
    if ((entry.FormatId == "SPEC" || entry.FormatId == "DPA") && extension == "part")
    {
        return Kind::SPEC_PARTICLE;
    }
    if ((entry.FormatId == "SPEC" || entry.FormatId == "DPA") && (extension == "img" || extension == "ftile" || extension == "btile"))
    {
        return Kind::SPEC_PICTURE; // Abuse: width and height, then the indices of the palette of the file
    }
    if (entry.FormatId == "ART" && entry.DataX > 0 && entry.DataY > 0)
    {
        return Kind::ART_TILE;
    }
    return Kind::NONE;
}

class Bytes
{
  public:
    explicit Bytes(std::span<const std::byte> data) : _data(data)
    {
    }
    [[nodiscard]] auto Size() const -> std::size_t
    {
        return _data.size();
    }
    [[nodiscard]] auto Has(std::size_t offset, std::size_t length) const -> bool
    {
        return offset <= _data.size() && length <= _data.size() - offset;
    }
    [[nodiscard]] auto U8(std::size_t at) const -> std::uint8_t
    {
        return static_cast<std::uint8_t>(_data[at]);
    }
    [[nodiscard]] auto U16(std::size_t at) const -> std::uint32_t
    {
        return U8(at) | (static_cast<std::uint32_t>(U8(at + 1)) << 8);
    }
    [[nodiscard]] auto U32(std::size_t at) const -> std::uint32_t
    {
        return U16(at) | (U16(at + 2) << 16);
    }
    [[nodiscard]] auto Ptr(std::size_t at) const -> const std::uint8_t *
    {
        return reinterpret_cast<const std::uint8_t *>(_data.data()) + at;
    }

  private:
    std::span<const std::byte> _data;
};

auto Indexed(const std::uint8_t *pixels, std::uint32_t width, std::uint32_t height, const PaletteRgb &palette, bool columnMajor = false) -> DecodedImage
{
    DecodedImage image;
    image.Width = width;
    image.Height = height;
    image.Rgba.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const std::uint8_t index = columnMajor ? pixels[static_cast<std::size_t>(x) * height + y] : pixels[static_cast<std::size_t>(y) * width + x];
            std::uint8_t *out = &image.Rgba[(static_cast<std::size_t>(y) * width + x) * 4];
            out[0] = palette[index * 3];
            out[1] = palette[index * 3 + 1];
            out[2] = palette[index * 3 + 2];
            out[3] = 255;
        }
    }
    return image;
}

auto Sane(std::uint32_t width, std::uint32_t height) -> bool
{
    return width > 0 && height > 0 && width <= 16384 && height <= 16384;
}

// A DDS file for a block compressed or raw payload: only what DecodeDds needs (the first mip level)
auto MakeDds(std::uint32_t width, std::uint32_t height, const char *fourCc, std::span<const std::byte> payload) -> std::vector<std::byte>
{
    std::vector<std::byte> dds(128, std::byte{0});
    auto put = [&](std::size_t at, std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
        {
            dds[at + static_cast<std::size_t>(i)] = static_cast<std::byte>(v >> (8 * i));
        }
    };
    std::memcpy(dds.data(), "DDS ", 4);
    put(4, 124);
    put(8, 0x1 | 0x2 | 0x4 | 0x1000);
    put(12, height);
    put(16, width);
    put(76, 32);
    put(80, 4); // DDPF_FOURCC
    std::memcpy(dds.data() + 84, fourCc, 4);
    put(108, 0x1000);
    dds.insert(dds.end(), payload.begin(), payload.end());
    return dds;
}

auto DecodeBlocks(std::uint32_t width, std::uint32_t height, const char *fourCc, std::span<const std::byte> payload) -> std::optional<DecodedImage>
{
    const std::size_t blockBytes = fourCc[3] == '1' ? 8 : 16;
    const std::size_t needed = static_cast<std::size_t>((width + 3) / 4) * ((height + 3) / 4) * blockBytes;
    if (!Sane(width, height) || payload.size() < needed)
    {
        return std::nullopt;
    }
    return DecodeDds(MakeDds(width, height, fourCc, payload)).Image;
}

auto DecodeWadPicture(const Bytes &b, const PaletteRgb &palette) -> std::optional<DecodedImage>
{
    if (!b.Has(0, 8))
    {
        return std::nullopt;
    }
    const std::uint32_t width = b.U32(0), height = b.U32(4);
    if (!Sane(width, height) || !b.Has(8, static_cast<std::size_t>(width) * height))
    {
        return std::nullopt;
    }
    return Indexed(b.Ptr(8), width, height, palette);
}

auto DecodeMipTex(const Bytes &b, const PaletteRgb *external) -> std::optional<DecodedImage>
{
    if (!b.Has(0, 40))
    {
        return std::nullopt;
    }
    const std::uint32_t width = b.U32(16), height = b.U32(20), offset = b.U32(24);
    if (!Sane(width, height) || !b.Has(offset, static_cast<std::size_t>(width) * height))
    {
        return std::nullopt;
    }
    PaletteRgb palette{};
    if (external != nullptr)
    {
        palette = *external;
    }
    else
    {
        // Half-Life: the palette is the last 768 bytes of the lump, after a 16 bits count
        if (b.Size() < 770 || b.Size() - 770 < static_cast<std::size_t>(offset) + static_cast<std::size_t>(width) * height)
        {
            return std::nullopt;
        }
        std::memcpy(palette.data(), b.Ptr(b.Size() - 768), 768);
    }
    return Indexed(b.Ptr(offset), width, height, palette);
}

auto DecodeConsole(const Bytes &b, const PaletteRgb &palette) -> std::optional<DecodedImage>
{
    if (b.Size() == 16384)
    {
        return Indexed(b.Ptr(0), 128, 128, palette);
    }
    if (b.Size() == 64000)
    {
        return Indexed(b.Ptr(0), 320, 200, palette);
    }
    return std::nullopt;
}

auto DecodePod3(const Bytes &b) -> std::optional<DecodedImage>
{
    if (!b.Has(0, 24 + 768))
    {
        return std::nullopt;
    }
    const std::uint32_t width = b.U32(8), height = b.U32(12);
    if (!Sane(width, height) || !b.Has(24 + 768, static_cast<std::size_t>(width) * height))
    {
        return std::nullopt;
    }
    PaletteRgb palette{};
    std::memcpy(palette.data(), b.Ptr(24), 768);
    return Indexed(b.Ptr(24 + 768), width, height, palette);
}

auto DecodePod5(const Bytes &b) -> std::optional<DecodedImage>
{
    constexpr std::size_t HEADER = 52;
    if (!b.Has(0, HEADER))
    {
        return std::nullopt;
    }
    const std::uint32_t format = b.U32(24), width = b.U32(28), height = b.U32(32);
    if (!Sane(width, height))
    {
        return std::nullopt;
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    const std::uint8_t *p = b.Ptr(HEADER);
    const std::size_t left = b.Size() - HEADER;
    const std::span<const std::byte> payload(reinterpret_cast<const std::byte *>(p), left);
    DecodedImage image;
    image.Width = width;
    image.Height = height;
    image.Rgba.resize(pixels * 4);
    switch (format)
    {
    case 3: // 32 bits, alpha red green blue (blue first in memory)
    case 24:
        if (left < pixels * 4)
        {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < pixels; ++i)
        {
            image.Rgba[i * 4] = p[i * 4 + 2];
            image.Rgba[i * 4 + 1] = p[i * 4 + 1];
            image.Rgba[i * 4 + 2] = p[i * 4];
            image.Rgba[i * 4 + 3] = p[i * 4 + 3];
        }
        return image;
    case 4: // 5:6:5
        if (left < pixels * 2)
        {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < pixels; ++i)
        {
            const std::uint32_t v = p[i * 2] | (static_cast<std::uint32_t>(p[i * 2 + 1]) << 8);
            image.Rgba[i * 4] = static_cast<std::uint8_t>(((v >> 11) & 31) * 255 / 31);
            image.Rgba[i * 4 + 1] = static_cast<std::uint8_t>(((v >> 5) & 63) * 255 / 63);
            image.Rgba[i * 4 + 2] = static_cast<std::uint8_t>((v & 31) * 255 / 31);
            image.Rgba[i * 4 + 3] = 255;
        }
        return image;
    case 5: // 4:4:4:4
        if (left < pixels * 2)
        {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < pixels; ++i)
        {
            const std::uint32_t v = p[i * 2] | (static_cast<std::uint32_t>(p[i * 2 + 1]) << 8);
            image.Rgba[i * 4] = static_cast<std::uint8_t>(((v >> 8) & 15) * 17);
            image.Rgba[i * 4 + 1] = static_cast<std::uint8_t>(((v >> 4) & 15) * 17);
            image.Rgba[i * 4 + 2] = static_cast<std::uint8_t>((v & 15) * 17);
            image.Rgba[i * 4 + 3] = static_cast<std::uint8_t>(((v >> 12) & 15) * 17);
        }
        return image;
    case 47: // luminance and alpha
        if (left < pixels * 2)
        {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < pixels; ++i)
        {
            image.Rgba[i * 4] = image.Rgba[i * 4 + 1] = image.Rgba[i * 4 + 2] = p[i * 2];
            image.Rgba[i * 4 + 3] = p[i * 2 + 1];
        }
        return image;
    case 55: // luminance
        if (left < pixels)
        {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < pixels; ++i)
        {
            image.Rgba[i * 4] = image.Rgba[i * 4 + 1] = image.Rgba[i * 4 + 2] = p[i];
            image.Rgba[i * 4 + 3] = 255;
        }
        return image;
    case 23:
        return DecodeBlocks(width, height, "DXT3", payload);
    case 43:
        return DecodeBlocks(width, height, "DXT1", payload);
    case 50:
        return DecodeBlocks(width, height, "DXT5", payload);
    default:
        return std::nullopt;
    }
}

auto Gunzip(std::span<const std::byte> data) -> std::optional<std::vector<std::byte>>
{
    z_stream stream{};
    if (inflateInit2(&stream, 16 + 15) != Z_OK)
    {
        return std::nullopt;
    }
    std::vector<std::byte> out;
    std::array<std::byte, 65536> buffer;
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<std::byte *>(data.data()));
    stream.avail_in = static_cast<uInt>(data.size());
    int status = Z_OK;
    while (status == Z_OK)
    {
        stream.next_out = reinterpret_cast<Bytef *>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(buffer.size() - stream.avail_out));
        if (out.size() > (256U << 20))
        {
            break;
        }
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END)
    {
        return std::nullopt;
    }
    return out;
}

auto DecodeKapl(const Bytes &b, std::span<const std::byte> all) -> std::optional<DecodedImage>
{
    if (!b.Has(0, 12) || (std::memcmp(b.Ptr(0), "DXT5", 4) != 0 && std::memcmp(b.Ptr(0), "DXT1", 4) != 0))
    {
        return std::nullopt;
    }
    const std::uint32_t width = b.U32(4), height = b.U32(8);
    char fourCc[5];
    std::memcpy(fourCc, b.Ptr(0), 4);
    fourCc[4] = 0;
    const auto payload = all.subspan(12);
    if (b.Size() > 14 && b.U16(12) == 0x8B1F)
    {
        const auto unpacked = Gunzip(payload);
        return unpacked ? DecodeBlocks(width, height, fourCc, *unpacked) : std::nullopt;
    }
    return DecodeBlocks(width, height, fourCc, payload);
}

// The header of the textures of Glacier: size, 2 types, index, height, width, mip maps, 3 unknown, then the name and the data
struct GlacierHeader
{
    std::uint32_t Height{0}, Width{0}, Mips{0};
    std::size_t DataAt{0}; // after the name and the size of the first block
    std::uint32_t BlockSize{0};
    char Type[5]{};
};

auto ReadGlacier(const Bytes &b) -> std::optional<GlacierHeader>
{
    if (!b.Has(0, 36))
    {
        return std::nullopt;
    }
    GlacierHeader h;
    std::memcpy(h.Type, b.Ptr(4), 4);
    if (std::memcmp(b.Ptr(4), b.Ptr(8), 4) != 0)
    {
        return std::nullopt;
    }
    h.Height = b.U16(16);
    h.Width = b.U16(18);
    h.Mips = b.U32(20);
    std::size_t at = 36;
    while (at < b.Size() && b.U8(at) != 0)
    {
        ++at;
    }
    ++at;
    if (!b.Has(at, 4))
    {
        return std::nullopt;
    }
    h.BlockSize = b.U32(at);
    h.DataAt = at + 4;
    if (!Sane(h.Width, h.Height) || !b.Has(h.DataAt, h.BlockSize))
    {
        return std::nullopt;
    }
    return h;
}

auto DecodeGlacier(Kind kind, const Bytes &b, std::span<const std::byte> all) -> std::optional<DecodedImage>
{
    const auto h = ReadGlacier(b);
    if (!h)
    {
        return std::nullopt;
    }
    const std::size_t pixels = static_cast<std::size_t>(h->Width) * h->Height;
    if (kind == Kind::GTEX_RGBA)
    {
        if (std::memcmp(h->Type, "ABGR", 4) != 0 || h->BlockSize < pixels * 4)
        {
            return std::nullopt;
        }
        DecodedImage image;
        image.Width = h->Width;
        image.Height = h->Height;
        image.Rgba.assign(b.Ptr(h->DataAt), b.Ptr(h->DataAt) + pixels * 4);
        return image;
    }
    if (kind == Kind::GTEX_PALN)
    {
        if (std::memcmp(h->Type, "NLAP", 4) != 0 || h->BlockSize < pixels || !b.Has(h->DataAt + h->BlockSize, 4))
        {
            return std::nullopt;
        }
        const std::uint32_t colours = b.U32(h->DataAt + h->BlockSize);
        const std::size_t paletteAt = h->DataAt + h->BlockSize + 4;
        if (colours == 0 || colours > 256 || !b.Has(paletteAt, static_cast<std::size_t>(colours) * 4))
        {
            return std::nullopt;
        }
        DecodedImage image;
        image.Width = h->Width;
        image.Height = h->Height;
        image.Rgba.resize(pixels * 4);
        for (std::size_t i = 0; i < pixels; ++i)
        {
            const std::uint8_t index = b.U8(h->DataAt + i);
            if (index >= colours)
            {
                return std::nullopt;
            }
            std::memcpy(&image.Rgba[i * 4], b.Ptr(paletteAt + static_cast<std::size_t>(index) * 4), 4);
        }
        return image;
    }
    // DXT1 / DXT3: the type is "1TXD" or "3TXD"
    const char dxt = h->Type[0];
    if ((dxt != '1' && dxt != '3') || std::memcmp(h->Type + 1, "TXD", 3) != 0)
    {
        return std::nullopt;
    }
    const char fourCc[5] = {'D', 'X', 'T', dxt, 0};
    return DecodeBlocks(h->Width, h->Height, fourCc, all.subspan(h->DataAt, h->BlockSize));
}
} // namespace

auto HasTextureExtension(std::string_view fileName) -> bool
{
    static const char *const known[] = {"psb", "pmm", "mm", "cp", "tex", "dxt", "rgba", "paln", "dxt1", "dxt3", "img", "ftile", "btile", "chr", "part"};
    const std::string extension = ExtensionOf(fileName);
    return std::any_of(std::begin(known), std::end(known), [&](const char *k) { return extension == k; });
}

auto IsTextureEntry(const TextureEntry &entry) -> bool
{
    return KindOf(entry) != Kind::NONE;
}

auto NeedsPalette(const TextureEntry &entry) -> bool
{
    const Kind kind = KindOf(entry);
    return kind == Kind::WAD_PICTURE || kind == Kind::WAD2_MIPTEX || kind == Kind::WAD_CONSOLE || kind == Kind::ART_TILE || kind == Kind::SPEC_PICTURE || kind == Kind::SPEC_CHARACTER || kind == Kind::SPEC_PARTICLE;
}

auto DecodeTexture(const TextureEntry &entry, std::span<const std::byte> bytes, const PaletteRgb *palette) -> std::optional<DecodedImage>
{
    const Kind kind = KindOf(entry);
    const Bytes b(bytes);
    switch (kind)
    {
    case Kind::NONE:
        return std::nullopt;
    case Kind::WAD3_MIPTEX:
        return DecodeMipTex(b, nullptr);
    case Kind::WAD2_MIPTEX:
        return palette != nullptr ? DecodeMipTex(b, palette) : std::nullopt;
    case Kind::WAD_PICTURE:
        return palette != nullptr ? DecodeWadPicture(b, *palette) : std::nullopt;
    case Kind::WAD_CONSOLE:
        return palette != nullptr ? DecodeConsole(b, *palette) : std::nullopt;
    case Kind::POD3_TEX:
        return DecodePod3(b);
    case Kind::POD5_TEX:
        return DecodePod5(b);
    case Kind::KAPL_DXT:
        return DecodeKapl(b, bytes);
    case Kind::GTEX_RGBA:
    case Kind::GTEX_PALN:
    case Kind::GTEX_DXT:
        return DecodeGlacier(kind, b, bytes);
    case Kind::SPEC_PICTURE:
    {
        if (palette == nullptr || !b.Has(0, 4))
        {
            return std::nullopt;
        }
        const std::uint32_t width = b.U16(0), height = b.U16(2);
        if (!Sane(width, height) || !b.Has(4, static_cast<std::size_t>(width) * height))
        {
            return std::nullopt;
        }
        return Indexed(b.Ptr(4), width, height, *palette);
    }
    case Kind::SPEC_CHARACTER:
    {
        if (palette == nullptr || !b.Has(0, 4))
        {
            return std::nullopt;
        }
        const std::uint32_t width = b.U16(0), height = b.U16(2);
        if (!Sane(width, height) || !b.Has(4, static_cast<std::size_t>(width) * height))
        {
            return std::nullopt;
        }
        auto image = Indexed(b.Ptr(4), width, height, *palette);
        for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i)
        {
            if (b.U8(4 + i) == 0)
            {
                image.Rgba[i * 4 + 3] = 0;
            }
        }
        return image;
    }
    case Kind::SPEC_PARTICLE:
    {
        // uint32 count, then count points of int16 x, int16 y, uint8 color: drawn on a transparent picture of their bounds
        if (palette == nullptr || !b.Has(0, 4))
        {
            return std::nullopt;
        }
        const std::uint32_t total = b.U32(0);
        if (total == 0 || total > 1000000 || !b.Has(4, static_cast<std::size_t>(total) * 5))
        {
            return std::nullopt;
        }
        std::int32_t minX = 1 << 30, minY = 1 << 30, maxX = -(1 << 30), maxY = -(1 << 30);
        for (std::uint32_t i = 0; i < total; ++i)
        {
            const auto x = static_cast<std::int16_t>(b.U16(4 + i * 5)), y = static_cast<std::int16_t>(b.U16(6 + i * 5));
            minX = std::min<std::int32_t>(minX, x);
            maxX = std::max<std::int32_t>(maxX, x);
            minY = std::min<std::int32_t>(minY, y);
            maxY = std::max<std::int32_t>(maxY, y);
        }
        const auto width = static_cast<std::uint32_t>(maxX - minX + 1), height = static_cast<std::uint32_t>(maxY - minY + 1);
        if (!Sane(width, height))
        {
            return std::nullopt;
        }
        DecodedImage image;
        image.Width = width;
        image.Height = height;
        image.Rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
        for (std::uint32_t i = 0; i < total; ++i)
        {
            const auto x = static_cast<std::int16_t>(b.U16(4 + i * 5)) - minX, y = static_cast<std::int16_t>(b.U16(6 + i * 5)) - minY;
            const std::uint8_t color = b.U8(8 + i * 5);
            std::uint8_t *out = &image.Rgba[(static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4];
            out[0] = (*palette)[color * 3];
            out[1] = (*palette)[color * 3 + 1];
            out[2] = (*palette)[color * 3 + 2];
            out[3] = 255;
        }
        return image;
    }
    case Kind::ART_TILE:
    {
        const auto width = static_cast<std::uint32_t>(entry.DataX), height = static_cast<std::uint32_t>(entry.DataY);
        if (palette == nullptr || !Sane(width, height) || !b.Has(0, static_cast<std::size_t>(width) * height))
        {
            return std::nullopt;
        }
        return Indexed(b.Ptr(0), width, height, *palette, true); // the columns of a tile are stored one after the other
    }
    }
    return std::nullopt;
}

auto ParsePalette(std::span<const std::byte> bytes) -> std::optional<PaletteRgb>
{
    PaletteRgb palette{};
    if (bytes.size() >= 8 && std::memcmp(bytes.data(), "JASC-PAL", 8) == 0)
    {
        std::istringstream text(std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
        std::string line;
        std::getline(text, line); // JASC-PAL
        std::getline(text, line); // 0100
        int count = 0;
        text >> count;
        if (count < 1 || count > 256)
        {
            return std::nullopt;
        }
        for (int i = 0; i < count; ++i)
        {
            int r = 0, g = 0, b = 0;
            if (!(text >> r >> g >> b))
            {
                return std::nullopt;
            }
            palette[static_cast<std::size_t>(i) * 3] = static_cast<std::uint8_t>(r);
            palette[static_cast<std::size_t>(i) * 3 + 1] = static_cast<std::uint8_t>(g);
            palette[static_cast<std::size_t>(i) * 3 + 2] = static_cast<std::uint8_t>(b);
        }
        return palette;
    }
    if (bytes.size() < 768)
    {
        return std::nullopt;
    }
    std::uint8_t most = 0;
    for (std::size_t i = 0; i < 768; ++i)
    {
        palette[i] = static_cast<std::uint8_t>(bytes[i]);
        most = std::max(most, palette[i]);
    }
    if (most < 64) // 6 bits per component
    {
        for (auto &v : palette)
        {
            v = static_cast<std::uint8_t>((v << 2) | (v >> 4));
        }
    }
    return palette;
}

auto IsPaletteEntryName(std::string_view name) -> bool
{
    std::string lower = Lower(name);
    const auto slash = lower.find_last_of("\\/");
    if (slash != std::string::npos)
    {
        lower.erase(0, slash + 1);
    }
    return lower == "palette.dat" || lower == "palette.lmp" || lower == "playpal" || lower == "playpal.lmp" || lower == "blood.pal" || lower == "palette.pal" || lower.rfind("palette.", 0) == 0;
}

namespace
{
// The palette entry of Abuse: uint16 count, then count red, green, blue triples
auto ParseSpecPalette(std::span<const std::byte> bytes) -> std::optional<PaletteRgb>
{
    if (bytes.size() < 2)
    {
        return std::nullopt;
    }
    const std::size_t count = std::to_integer<unsigned>(bytes[0]) | (std::to_integer<unsigned>(bytes[1]) << 8);
    if (count < 1 || count > 256 || bytes.size() < 2 + count * 3)
    {
        return std::nullopt;
    }
    PaletteRgb palette{};
    for (std::size_t i = 0; i < count * 3; ++i)
    {
        palette[i] = std::to_integer<std::uint8_t>(bytes[2 + i]);
    }
    return palette;
}
} // namespace

auto FindPaletteInArchive(const Dudi7::IArchive &archive) -> std::optional<PaletteRgb>
{
    const auto count = archive.GetEntryCount();
    if (archive.GetFormatId() == "SPEC" || archive.GetFormatId() == "DPA")
    {
        for (std::int64_t i = 0; i < count && i < 2000000; ++i)
        {
            const auto info = archive.GetEntry(i);
            if (info.DataX != 2 || info.Size < 2 || info.Size > (1 << 20)) // the entries of type palette
            {
                continue;
            }
            std::vector<std::byte> bytes;
            if (archive.Extract(i, [&](std::span<const std::byte> piece) {
                    bytes.insert(bytes.end(), piece.begin(), piece.end());
                    return true;
                }) == 0)
            {
                if (auto palette = ParseSpecPalette(bytes))
                {
                    return palette;
                }
            }
        }
        return std::nullopt;
    }
    for (std::int64_t i = 0; i < count && i < 2000000; ++i)
    {
        const auto info = archive.GetEntry(i);
        if (info.Size < 768 || info.Size > (1 << 20) || !IsPaletteEntryName(info.Name))
        {
            continue;
        }
        std::vector<std::byte> bytes;
        if (archive.Extract(i, [&](std::span<const std::byte> piece) {
                bytes.insert(bytes.end(), piece.begin(), piece.end());
                return true;
            }) != 0)
        {
            continue;
        }
        if (auto palette = ParsePalette(bytes))
        {
            return palette;
        }
    }
    return std::nullopt;
}

auto LoadPaletteFile(const std::string &path) -> std::optional<PaletteRgb>
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return std::nullopt;
    }
    std::vector<char> raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return ParsePalette(std::as_bytes(std::span(raw)));
}
} // namespace Dup::Core
