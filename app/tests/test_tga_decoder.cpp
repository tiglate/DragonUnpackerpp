// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/TgaDecoder.h"

#include "ThirdParty/doctest.h"

#include <array>
#include <vector>

using namespace Dup::Core;

namespace
{
auto Header(std::uint8_t type, std::uint8_t depth, std::uint8_t descriptor, std::uint16_t width, std::uint16_t height) -> std::vector<std::byte>
{
    std::vector<std::byte> bytes(18);
    bytes[2] = std::byte{type};
    bytes[12] = std::byte{static_cast<std::uint8_t>(width & 255)};
    bytes[13] = std::byte{static_cast<std::uint8_t>(width >> 8)};
    bytes[14] = std::byte{static_cast<std::uint8_t>(height & 255)};
    bytes[15] = std::byte{static_cast<std::uint8_t>(height >> 8)};
    bytes[16] = std::byte{depth};
    bytes[17] = std::byte{descriptor};
    return bytes;
}

void Append(std::vector<std::byte> &bytes, std::initializer_list<int> values)
{
    for (const int v : values)
    {
        bytes.push_back(std::byte{static_cast<std::uint8_t>(v)});
    }
}
} // namespace

TEST_CASE("32 bits TGA, bottom left origin: rows are flipped, BGRA becomes RGBA")
{
    auto bytes = Header(2, 32, 0x08, 1, 2);
    Append(bytes, {10, 20, 30, 40}); // bottom row (B, G, R, A)
    Append(bytes, {1, 2, 3, 4});     // top row
    const auto image = DecodeTga(bytes);
    REQUIRE(image.has_value());
    CHECK(image->Width == 1);
    CHECK(image->Height == 2);
    CHECK(image->Rgba == std::vector<std::uint8_t>{3, 2, 1, 4, 30, 20, 10, 40});
}

TEST_CASE("Top left origin keeps the order, gray 8 bits")
{
    auto bytes = Header(3, 8, 0x20, 2, 1);
    Append(bytes, {7, 9});
    const auto image = DecodeTga(bytes);
    REQUIRE(image.has_value());
    CHECK(image->Rgba == std::vector<std::uint8_t>{7, 7, 7, 255, 9, 9, 9, 255});
}

TEST_CASE("RLE packets")
{
    auto bytes = Header(10, 24, 0x20, 4, 1);
    Append(bytes, {0x82, 1, 2, 3});          // 3 times (B,G,R) = (1,2,3)
    Append(bytes, {0x00, 9, 8, 7});          // one raw pixel
    const auto image = DecodeTga(bytes);
    REQUIRE(image.has_value());
    CHECK(image->Rgba[0] == 3);
    CHECK(image->Rgba[8] == 3);
    CHECK(image->Rgba[12] == 7);
}

TEST_CASE("Truncated or unsupported files are refused")
{
    CHECK_FALSE(DecodeTga(std::vector<std::byte>(5)).has_value());
    auto truncated = Header(2, 32, 0x20, 4, 4);
    Append(truncated, {1, 2, 3, 4});
    CHECK_FALSE(DecodeTga(truncated).has_value());
    CHECK_FALSE(DecodeTga(Header(1, 8, 0, 1, 1)).has_value()); // colour mapped
    CHECK_FALSE(DecodeTga(Header(2, 32, 0, 0, 1)).has_value()); // no width
}

namespace
{
/// A 3x1 colour mapped image: indices 2, 0, 1 and a map of 3 colours of the given depth (24: B, G, R)
auto MappedImage(std::uint8_t type, bool rle) -> std::vector<std::byte>
{
    auto bytes = Header(type, 8, 0x20, 3, 1);
    bytes[1] = std::byte{1};  // colour map
    bytes[5] = std::byte{3};  // 3 entries
    bytes[7] = std::byte{24}; // of 24 bits
    Append(bytes, {10, 20, 30});  // 0: R30 G20 B10
    Append(bytes, {40, 50, 60});  // 1: R60 G50 B40
    Append(bytes, {70, 80, 90});  // 2: R90 G80 B70
    if (rle)
    {
        Append(bytes, {0x02, 2, 0, 1}); // a raw packet of 3 pixels
    }
    else
    {
        Append(bytes, {2, 0, 1});
    }
    return bytes;
}
} // namespace

TEST_CASE("colour mapped TGA (8 bits indices, 24 bits map), raw and RLE")
{
    for (const bool rle : {false, true})
    {
        const auto image = DecodeTga(MappedImage(rle ? 9 : 1, rle));
        REQUIRE(image.has_value());
        CHECK(image->Width == 3);
        CHECK(image->Height == 1);
        const std::vector<std::uint8_t> expected{90, 80, 70, 255, 30, 20, 10, 255, 60, 50, 40, 255};
        CHECK(image->Rgba == expected);
    }
}

TEST_CASE("a palette replaces the map of a colour mapped TGA, and does nothing to the other images")
{
    std::array<std::uint8_t, 768> palette{};
    palette[0] = 1;
    palette[1] = 2;
    palette[2] = 3; // colour 0
    palette[3] = 4;
    palette[4] = 5;
    palette[5] = 6; // colour 1
    palette[6] = 7;
    palette[7] = 8;
    palette[8] = 9; // colour 2
    const auto image = DecodeTga(MappedImage(1, false), &palette);
    REQUIRE(image.has_value());
    const std::vector<std::uint8_t> expected{7, 8, 9, 255, 1, 2, 3, 255, 4, 5, 6, 255};
    CHECK(image->Rgba == expected);

    auto gray = Header(3, 8, 0x20, 1, 1);
    Append(gray, {77});
    const auto plain = DecodeTga(gray, &palette);
    REQUIRE(plain.has_value());
    CHECK(plain->Rgba[0] == 77);
}

TEST_CASE("colour mapped TGA: a map that is missing or truncated is refused")
{
    auto noMap = MappedImage(1, false);
    noMap[1] = std::byte{0}; // says there is no colour map
    CHECK_FALSE(DecodeTga(noMap).has_value());
    auto truncated = MappedImage(1, false);
    truncated.resize(25); // inside the map
    CHECK_FALSE(DecodeTga(truncated).has_value());
}
