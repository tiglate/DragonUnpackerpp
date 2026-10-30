// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Palette.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <set>
#include <string>

namespace Dud
{
namespace
{
// The levels of one channel of the Mega Drive video chip (3 bits)
constexpr std::array<uint8_t, 8> MdLevels = {0, 52, 87, 116, 144, 172, 206, 255};

// The usual picture of the NES colors (the chip makes them from a signal: any table is an approximation)
constexpr std::array<uint32_t, 64> NesColors = {
    0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400, 0x503000, 0x007800, 0x006800,
    0x005800, 0x004058, 0x000000, 0x000000, 0x000000, 0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058,
    0xF83800, 0xE45C10, 0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888, 0x000000, 0x000000, 0x000000, 0xF8F8F8,
    0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044, 0xF8B800, 0xB8F818, 0x58D854, 0x58F898,
    0x00E8D8, 0x787878, 0x000000, 0x000000, 0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0,
    0xFCE0A8, 0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000, 0x000000};

auto Be16(const uint8_t *p) -> unsigned
{
    return (unsigned{p[0]} << 8) | p[1];
}

auto Le16(const uint8_t *p) -> unsigned
{
    return unsigned{p[0]} | (unsigned{p[1]} << 8);
}

void Put16(std::vector<uint8_t> &out, size_t at, int value)
{
    out[at] = static_cast<uint8_t>(value & 255);
    out[at + 1] = static_cast<uint8_t>(value >> 8);
}
} // namespace

auto BytesPerColor(PaletteKind kind) -> int
{
    return kind == PaletteKind::Nes ? 1 : 2;
}

auto FindMegaDrivePalettes(const uint8_t *rom, size_t size) -> std::vector<PaletteFind>
{
    std::vector<PaletteFind> found;
    constexpr size_t Line = 16;
    for (size_t at = 0; at + 2 * Line <= size;)
    {
        // the run of words that are valid colors from here
        size_t words = 0;
        while (at + 2 * (words + 1) <= size && (Be16(rom + at + 2 * words) & 0xF111) == 0)
            ++words;
        if (words < Line || (at & 1) != 0)
        {
            at += (words > 0 && (at & 1) == 0) ? 2 * words : 2;
            continue;
        }
        const size_t lines = words / Line;
        // zero filled areas and tables of a couple of values are not palettes: a line needs colors
        for (size_t l = 0; l < lines; ++l)
        {
            std::set<unsigned> distinct;
            size_t zeros = 0;
            for (size_t i = 0; i < Line; ++i)
            {
                const unsigned w = Be16(rom + at + 2 * (l * Line + i));
                distinct.insert(w);
                zeros += w == 0 ? 1 : 0;
            }
            if (distinct.size() >= 4 && zeros <= Line / 2)
                found.push_back({PaletteKind::MegaDrive, static_cast<int64_t>(at + 2 * l * Line), static_cast<int>(Line)});
        }
        at += 2 * words;
    }
    return found;
}

namespace
{
// Does the line of 16 words at data look like a Super Nintendo palette? (a tile map looks like one: tile
// numbers counting up, the top bit that is the vertical flip clear; so do tables of small numbers)
auto SnesLineOk(const uint8_t *data) -> bool
{
    unsigned w[16];
    for (int i = 0; i < 16; ++i)
    {
        w[i] = Le16(data + 2 * i);
        if ((w[i] & 0x8000) != 0)
            return false;
    }
    std::set<unsigned> distinct(w, w + 16);
    if (distinct.size() < 8)
        return false;
    int zeros = 0;
    int counting = 0;
    int same = 0;
    int steps = 0;
    for (int i = 0; i < 16; ++i)
        zeros += w[i] == 0 ? 1 : 0;
    for (int i = 0; i < 15; ++i)
    {
        counting += w[i + 1] == w[i] + 1 ? 1 : 0;
        same += w[i + 1] == w[i] ? 1 : 0;
        steps += std::abs(static_cast<int>(w[i + 1] & 31) - static_cast<int>(w[i] & 31)) +
                 std::abs(static_cast<int>((w[i + 1] >> 5) & 31) - static_cast<int>((w[i] >> 5) & 31)) +
                 std::abs(static_cast<int>((w[i + 1] >> 10) & 31) - static_cast<int>((w[i] >> 10) & 31));
    }
    std::set<unsigned> red;
    std::set<unsigned> green;
    std::set<unsigned> blue;
    for (const unsigned c : w)
    {
        red.insert(c & 31);
        green.insert((c >> 5) & 31);
        blue.insert((c >> 10) & 31);
    }
    return zeros <= 6 && counting < 4 && same < 5 && std::min({red.size(), green.size(), blue.size()}) >= 3 && steps <= 15 * 20;
}
} // namespace

auto FindSnesPalettes(const uint8_t *rom, size_t size) -> std::vector<PaletteFind>
{
    // A single line proves little: 8,000 of them are in a ROM that has a few hundred. Games keep their
    // palettes in blocks of many lines, which the junk does not: only runs of consecutive lines count.
    constexpr size_t Line = 32; // bytes
    constexpr size_t MinimumLines = 8;
    std::vector<PaletteFind> found;
    if (size < Line * MinimumLines)
        return found;
    std::vector<uint8_t> ok(size / 2, 0);
    for (size_t at = 0; at + Line <= size; at += 2)
        ok[at / 2] = SnesLineOk(rom + at) ? 1 : 0;
    size_t covered = 0; // the offsets before this one belong to a block already found
    for (size_t at = 0; at + Line <= size; at += 2)
    {
        if (at < covered || ok[at / 2] == 0)
            continue;
        size_t lines = 0;
        while (at + (lines + 1) * Line <= size && ok[(at + lines * Line) / 2] != 0)
            ++lines;
        if (lines < MinimumLines)
            continue;
        for (size_t l = 0; l < lines; ++l)
            found.push_back({PaletteKind::Snes, static_cast<int64_t>(at + l * Line), 16});
        covered = at + lines * Line;
    }
    return found;
}

auto FindNesPalettes(const uint8_t *rom, size_t size) -> std::vector<PaletteFind>
{
    // 16 bytes (4 groups of 4 colors) whose first colors are the same background color, then the 16 of the
    // sprites when they look the same. Each group is a palette of the tiles (2 bits per pixel), so each is
    // reported on its own.
    std::vector<PaletteFind> found;
    for (size_t at = 0; at + 16 <= size; ++at)
    {
        bool ok = true;
        for (size_t i = 0; ok && i < 16; ++i)
            ok = rom[at + i] < 0x40;
        if (!ok || rom[at] != rom[at + 4] || rom[at] != rom[at + 8] || rom[at] != rom[at + 12])
            continue;
        // every group has colors of its own (tables of small numbers have groups of one value)
        bool groupsOk = true;
        for (size_t g = 0; groupsOk && g < 4; ++g)
        {
            const std::set<unsigned> group(rom + at + 4 * g, rom + at + 4 * g + 4);
            groupsOk = group.size() >= 3;
        }
        if (!groupsOk)
            continue;
        bool whole = at + 32 <= size;
        for (size_t i = 16; whole && i < 32; ++i)
            whole = rom[at + i] < 0x40;
        whole = whole && rom[at + 16] == rom[at + 20] && rom[at + 16] == rom[at + 24] && rom[at + 16] == rom[at + 28];
        const size_t groups = whole ? 8 : 4;
        for (size_t g = 0; g < groups; ++g)
            found.push_back({PaletteKind::Nes, static_cast<int64_t>(at + 4 * g), 4});
        at += 4 * groups - 1;
    }
    return found;
}

auto PaletteColors(PaletteKind kind, const uint8_t *data, int count) -> std::vector<uint32_t>
{
    std::vector<uint32_t> colors;
    for (int i = 0; i < count; ++i)
    {
        unsigned r = 0;
        unsigned g = 0;
        unsigned b = 0;
        switch (kind)
        {
        case PaletteKind::MegaDrive: {
            const unsigned w = Be16(data + 2 * i);
            r = MdLevels[(w >> 1) & 7];
            g = MdLevels[(w >> 5) & 7];
            b = MdLevels[(w >> 9) & 7];
            break;
        }
        case PaletteKind::Snes: {
            const unsigned w = Le16(data + 2 * i);
            const auto expand = [](unsigned v) { return (v << 3) | (v >> 2); };
            r = expand(w & 31);
            g = expand((w >> 5) & 31);
            b = expand((w >> 10) & 31);
            break;
        }
        case PaletteKind::Nes: {
            const uint32_t rgb = NesColors[data[i] & 0x3F];
            r = (rgb >> 16) & 255;
            g = (rgb >> 8) & 255;
            b = rgb & 255;
            break;
        }
        }
        colors.push_back(0xFF000000u | (r << 16) | (g << 8) | b);
    }
    return colors;
}

auto MakeJascPal(const std::vector<uint32_t> &colors) -> std::vector<uint8_t>
{
    std::string text = "JASC-PAL\r\n0100\r\n" + std::to_string(colors.size()) + "\r\n";
    for (const uint32_t c : colors)
        text += std::to_string((c >> 16) & 255) + " " + std::to_string((c >> 8) & 255) + " " + std::to_string(c & 255) + "\r\n";
    return {text.begin(), text.end()};
}

auto MakeSwatchTga(const std::vector<uint32_t> &colors) -> std::vector<uint8_t>
{
    constexpr int Square = 16;
    constexpr int PerRow = 16;
    const int rows = (static_cast<int>(colors.size()) + PerRow - 1) / PerRow;
    const int width = PerRow * Square;
    const int height = std::max(1, rows) * Square;
    std::vector<uint8_t> tga(static_cast<size_t>(18 + width * height * 4), 0);
    tga[2] = 2;
    Put16(tga, 12, width);
    Put16(tga, 14, height);
    tga[16] = 32;
    tga[17] = 0x28;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const size_t index = static_cast<size_t>(y / Square * PerRow + x / Square);
            const uint32_t c = index < colors.size() ? colors[index] : 0xFF202020u;
            const size_t at = static_cast<size_t>(18 + (y * width + x) * 4);
            tga[at] = static_cast<uint8_t>(c & 255);
            tga[at + 1] = static_cast<uint8_t>((c >> 8) & 255);
            tga[at + 2] = static_cast<uint8_t>((c >> 16) & 255);
            tga[at + 3] = 255;
        }
    return tga;
}
} // namespace Dud
