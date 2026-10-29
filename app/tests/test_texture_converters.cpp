// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/TextureConverters.h"
#include "TestSupport.h"

#include <zlib.h>

#include <cstring>

using namespace Dup::Core;
using Blob = std::vector<std::uint8_t>;

namespace
{
void Le32(Blob &b, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
    {
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}
void Le16(Blob &b, std::uint32_t v)
{
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void Text(Blob &b, const std::string &t)
{
    b.insert(b.end(), t.begin(), t.end());
}
auto Span(const Blob &b) -> std::span<const std::byte>
{
    return std::as_bytes(std::span(b));
}

// colour i = (i, 255 - i, 7)
auto TestPalette() -> PaletteRgb
{
    PaletteRgb p{};
    for (int i = 0; i < 256; ++i)
    {
        p[static_cast<std::size_t>(i) * 3] = static_cast<std::uint8_t>(i);
        p[static_cast<std::size_t>(i) * 3 + 1] = static_cast<std::uint8_t>(255 - i);
        p[static_cast<std::size_t>(i) * 3 + 2] = 7;
    }
    return p;
}

auto Rgb(const DecodedImage &image, std::uint32_t x, std::uint32_t y) -> std::array<int, 4>
{
    const auto *p = &image.Rgba[(static_cast<std::size_t>(y) * image.Width + x) * 4];
    return {p[0], p[1], p[2], p[3]};
}
} // namespace

TEST_CASE("Quake pictures and the textures of Half-Life")
{
    const PaletteRgb palette = TestPalette();
    // a picture of 2x2
    Blob pic;
    Le32(pic, 2);
    Le32(pic, 2);
    pic.insert(pic.end(), {10, 20, 30, 40});
    TextureEntry entry{"WAD2", "ANUM_0.psb", 0, 0};
    CHECK(IsTextureEntry(entry));
    CHECK(NeedsPalette(entry));
    CHECK(!DecodeTexture(entry, Span(pic), nullptr));
    auto image = DecodeTexture(entry, Span(pic), &palette);
    REQUIRE(image);
    CHECK(image->Width == 2);
    CHECK(Rgb(*image, 1, 1) == std::array<int, 4>{40, 215, 7, 255});
    CHECK(Rgb(*image, 0, 1) == std::array<int, 4>{30, 225, 7, 255});

    // the console picture: 128 x 128 raw
    Blob console(16384, 5);
    entry = {"WAD2", "CONCHARS.mm", 0, 0};
    image = DecodeTexture(entry, Span(console), &palette);
    REQUIRE(image);
    CHECK(image->Width == 128);

    // a miptex of Half-Life (WAD3): 4x2 pixels, its palette at the end after a count of 256
    Blob mip;
    Text(mip, "WALL");
    mip.resize(16, 0);
    Le32(mip, 4);
    Le32(mip, 2);
    Le32(mip, 40); // offset of the pixels
    Le32(mip, 0);
    Le32(mip, 0);
    Le32(mip, 0);
    mip.insert(mip.end(), {0, 1, 2, 3, 4, 5, 6, 7});
    Le16(mip, 256);
    for (int i = 0; i < 256; ++i)
    {
        mip.insert(mip.end(), {static_cast<std::uint8_t>(i), 100, 200});
    }
    entry = {"WAD3", "WALL.pmm", 0, 0};
    CHECK(!NeedsPalette(entry));
    image = DecodeTexture(entry, Span(mip), nullptr);
    REQUIRE(image);
    CHECK(image->Width == 4);
    CHECK(Rgb(*image, 3, 1) == std::array<int, 4>{7, 100, 200, 255});
    CHECK(!IsTextureEntry({"PACK", "WALL.pmm", 0, 0}));
}

TEST_CASE("Build tiles are stored by columns")
{
    const PaletteRgb palette = TestPalette();
    // 2 wide, 3 tall: column 0 = 1 2 3, column 1 = 4 5 6
    const Blob tile{1, 2, 3, 4, 5, 6};
    const TextureEntry entry{"ART", "texture #000001.tex", 2, 3};
    CHECK(NeedsPalette(entry));
    const auto image = DecodeTexture(entry, Span(tile), &palette);
    REQUIRE(image);
    CHECK(image->Width == 2);
    CHECK(image->Height == 3);
    CHECK(Rgb(*image, 0, 0)[0] == 1);
    CHECK(Rgb(*image, 0, 2)[0] == 3);
    CHECK(Rgb(*image, 1, 0)[0] == 4);
    CHECK(Rgb(*image, 1, 2)[0] == 6);
}

TEST_CASE("Abuse images and tiles: width, height, then the indices of the palette")
{
    const PaletteRgb palette = TestPalette();
    // 3 wide, 2 tall, row by row, and two bytes of metadata after the pixels (a tile)
    const Blob image3x2{3, 0, 2, 0, 1, 2, 3, 4, 5, 6, 99, 99};
    for (const char *name : {"Image/title.img", "Fore tile/a.ftile", "Back tile/b.btile"})
    {
        const TextureEntry entry{"SPEC", name, 4, 0};
        CHECK(IsTextureEntry(entry));
        CHECK(NeedsPalette(entry));
        const auto image = DecodeTexture(entry, Span(image3x2), &palette);
        REQUIRE(image);
        CHECK(image->Width == 3);
        CHECK(image->Height == 2);
        CHECK(Rgb(*image, 2, 0)[0] == 3);
        CHECK(Rgb(*image, 0, 1)[0] == 4);
        CHECK(!DecodeTexture(entry, Span(image3x2), nullptr)); // no palette: nothing to show
    }
    CHECK(IsTextureEntry({"DPA", "Image/x.img", 4, 0}));
    CHECK(!IsTextureEntry({"SPEC", "Palette/palette", 2, 0}));
    const Blob truncated{3, 0, 2, 0, 1, 2};
    CHECK(!DecodeTexture({"SPEC", "Image/t.img", 4, 0}, Span(truncated), &palette));
}

TEST_CASE("Abuse characters (transparent index 0) and particle frames")
{
    const PaletteRgb palette = TestPalette();
    const Blob sprite{2, 0, 1, 0, 0, 7, 1, 2, 3}; // 2x1: transparent, 7; then the metadata
    const TextureEntry entry{"SPEC", "Character2/ant.chr", 21, 0};
    const auto image = DecodeTexture(entry, Span(sprite), &palette);
    REQUIRE(image);
    CHECK(Rgb(*image, 0, 0)[3] == 0);
    CHECK(Rgb(*image, 1, 0) == std::array<int, 4>{palette[21], palette[22], palette[23], 255});
    // two points: (-1, 2) of color 5 and (1, 3) of color 6 -> 3x2 picture
    Blob particle;
    Le32(particle, 2);
    for (const int value : {-1, 2})
    {
        particle.push_back(static_cast<std::uint8_t>(value & 255));
        particle.push_back(static_cast<std::uint8_t>((value >> 8) & 255));
    }
    particle.push_back(5);
    for (const int value : {1, 3})
    {
        particle.push_back(static_cast<std::uint8_t>(value & 255));
        particle.push_back(static_cast<std::uint8_t>((value >> 8) & 255));
    }
    particle.push_back(6);
    const auto frame = DecodeTexture({"SPEC", "Particle/spark.part", 22, 0}, Span(particle), &palette);
    REQUIRE(frame);
    CHECK(frame->Width == 3);
    CHECK(frame->Height == 2);
    CHECK(Rgb(*frame, 0, 0)[3] == 255);
    CHECK(Rgb(*frame, 1, 0)[3] == 0);
    CHECK(Rgb(*frame, 2, 1)[0] == palette[18]);
}

TEST_CASE("BloodRayne, Monkey Island and Hitman textures")
{
    // POD3: 6 integers, 768 bytes of palette, the pixels
    Blob pod3;
    Le32(pod3, 2);
    Le32(pod3, 0);
    Le32(pod3, 2);
    Le32(pod3, 2);
    Le32(pod3, 0);
    Le32(pod3, 0);
    for (int i = 0; i < 256; ++i)
    {
        pod3.insert(pod3.end(), {static_cast<std::uint8_t>(i), 0, 1});
    }
    pod3.insert(pod3.end(), {3, 2, 1, 0});
    auto image = DecodeTexture({"POD3", "a.TEX", 0, 0}, Span(pod3), nullptr);
    REQUIRE(image);
    CHECK(Rgb(*image, 0, 0)[0] == 3);
    CHECK(Rgb(*image, 1, 1)[0] == 0);

    // POD5: a 2x1 image of 32 bits (format 3, blue first in memory)
    Blob pod5;
    Le32(pod5, 0);
    for (int i = 0; i < 4; ++i)
    {
        Le32(pod5, 0);
    }
    Le32(pod5, 0);
    Le32(pod5, 3);
    Le32(pod5, 2);
    Le32(pod5, 1);
    Le32(pod5, 0);
    Le32(pod5, 0);
    Le32(pod5, 0);
    Le32(pod5, 0);
    pod5.insert(pod5.end(), {1, 2, 3, 4, 5, 6, 7, 8});
    image = DecodeTexture({"POD5", "a.TEX", 0, 0}, Span(pod5), nullptr);
    REQUIRE(image);
    CHECK(Rgb(*image, 0, 0) == std::array<int, 4>{3, 2, 1, 4});
    CHECK(Rgb(*image, 1, 0) == std::array<int, 4>{7, 6, 5, 8});

    // Monkey Island: "DXT1", width, height, then the blocks (also gzip compressed): one block, solid red 565
    Blob block{0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0};
    Blob dxt;
    Text(dxt, "DXT1");
    Le32(dxt, 4);
    Le32(dxt, 4);
    dxt.insert(dxt.end(), block.begin(), block.end());
    image = DecodeTexture({"KAPL", "a.dxt", 0, 0}, Span(dxt), nullptr);
    REQUIRE(image);
    CHECK(Rgb(*image, 2, 2)[0] >= 240);
    CHECK(Rgb(*image, 2, 2)[1] == 0);
    Blob packed(100);
    z_stream z{};
    deflateInit2(&z, 9, Z_DEFLATED, 16 + 15, 8, Z_DEFAULT_STRATEGY);
    z.next_in = block.data();
    z.avail_in = static_cast<uInt>(block.size());
    z.next_out = packed.data();
    z.avail_out = static_cast<uInt>(packed.size());
    deflate(&z, Z_FINISH);
    packed.resize(packed.size() - z.avail_out);
    deflateEnd(&z);
    Blob dxtGz(dxt.begin(), dxt.begin() + 12);
    dxtGz.insert(dxtGz.end(), packed.begin(), packed.end());
    image = DecodeTexture({"KAPL", "a.dxt", 0, 0}, Span(dxtGz), nullptr);
    REQUIRE(image);
    CHECK(Rgb(*image, 0, 0)[0] >= 240);

    // Glacier: 36 bytes header (size, two types, index, height, width, mip maps, 3 unknown), the name, the size of the block, the block
    auto glacier = [](const char *type, int height, int width, const std::string &name, const Blob &data) {
        Blob b;
        Le32(b, static_cast<std::uint32_t>(data.size()));
        Text(b, type);
        Text(b, type);
        Le32(b, 0);
        Le16(b, static_cast<std::uint32_t>(height));
        Le16(b, static_cast<std::uint32_t>(width));
        Le32(b, 1);
        Le32(b, 0);
        Le32(b, 0);
        Le32(b, 0);
        Text(b, name);
        b.push_back(0);
        Le32(b, static_cast<std::uint32_t>(data.size()));
        b.insert(b.end(), data.begin(), data.end());
        return b;
    };
    const Blob rgba = glacier("ABGR", 1, 2, "x", {1, 2, 3, 4, 5, 6, 7, 8});
    image = DecodeTexture({"GTEX", "x.RGBA", 0, 0}, Span(rgba), nullptr);
    REQUIRE(image);
    CHECK(Rgb(*image, 1, 0) == std::array<int, 4>{5, 6, 7, 8});
    Blob paln = glacier("NLAP", 1, 2, "x", {1, 0});
    Le32(paln, 2); // two colours, after the block
    paln.insert(paln.end(), {10, 20, 30, 40, 50, 60, 70, 80});
    image = DecodeTexture({"GTEX", "x.PALN", 0, 0}, Span(paln), nullptr);
    REQUIRE(image);
    CHECK(Rgb(*image, 0, 0) == std::array<int, 4>{50, 60, 70, 80});
    CHECK(Rgb(*image, 1, 0) == std::array<int, 4>{10, 20, 30, 40});
    const Blob dxt1 = glacier("1TXD", 4, 4, "x", block);
    image = DecodeTexture({"GTEX", "x.DXT1", 0, 0}, Span(dxt1), nullptr);
    REQUIRE(image);
    CHECK(Rgb(*image, 3, 3)[0] >= 240);
    // not what the name says
    CHECK(!DecodeTexture({"GTEX", "x.RGBA", 0, 0}, Span(paln), nullptr));
}

TEST_CASE("Palette files")
{
    // 6 bits per component (PALETTE.DAT of Build), 768 bytes and more
    Blob six(1000, 0);
    for (int i = 0; i < 768; ++i)
    {
        six[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i % 64);
    }
    auto palette = ParsePalette(Span(six));
    REQUIRE(palette);
    CHECK((*palette)[63] == 255);
    // 8 bits
    Blob eight(768, 200);
    palette = ParsePalette(Span(eight));
    REQUIRE(palette);
    CHECK((*palette)[5] == 200);
    // JASC
    const std::string jasc = "JASC-PAL\r\n0100\r\n2\r\n1 2 3\r\n4 5 6\r\n";
    palette = ParsePalette(std::as_bytes(std::span(jasc)));
    REQUIRE(palette);
    CHECK((*palette)[3] == 4);
    CHECK(!ParsePalette(Span(Blob(100, 1))));
    CHECK(IsPaletteEntryName("gfx/palette.lmp"));
    CHECK(IsPaletteEntryName("PALETTE.DAT"));
    CHECK(IsPaletteEntryName("PLAYPAL"));
    CHECK(!IsPaletteEntryName("palatial.txt"));
}
