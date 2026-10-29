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
