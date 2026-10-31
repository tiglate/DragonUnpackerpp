// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "codecs/RomTables.h"

#include <cstdlib>
#include <sstream>

#include "codecs/RomTables.inc"

namespace Dud
{
namespace
{
auto ParseBytes(const std::string &hex) -> std::vector<uint8_t>
{
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 2 <= hex.size(); i += 2)
        bytes.push_back(static_cast<uint8_t>(std::strtoul(hex.substr(i, 2).c_str(), nullptr, 16)));
    return bytes;
}

auto ParsePalette(const std::string &text) -> std::vector<uint32_t>
{
    std::vector<uint32_t> colors;
    for (size_t i = 0; i + 6 <= text.size(); i += 6)
        colors.push_back(static_cast<uint32_t>(std::strtoul(text.substr(i, 6).c_str(), nullptr, 16)));
    return colors;
}
} // namespace

auto FindKnownGame(uint32_t crc32, KnownGame &game) -> bool
{
    char wanted[16];
    std::snprintf(wanted, sizeof wanted, "%08x ", crc32);
    for (const char *text : KnownGameTexts)
    {
        const std::string all = text;
        if (all.compare(0, 9, wanted) != 0)
            continue;
        game = {};
        std::istringstream lines(all);
        std::string line;
        std::getline(lines, line);
        while (std::getline(lines, line))
        {
            std::istringstream fields(line);
            std::string kind;
            fields >> kind;
            if (kind == "S")
            {
                KnownStream stream;
                std::string word;
                std::string bpps;
                fields >> stream.Codec >> stream.Offset >> bpps;
                for (size_t i = 0; i < bpps.size(); i += 2)
                    stream.Bpp.push_back(bpps[i] - '0');
                while (fields >> word)
                    stream.Palettes.push_back(ParsePalette(word));
                game.Streams.push_back(std::move(stream));
            }
            else if (kind == "Q")
            {
                std::string word;
                while (fields >> word)
                    game.RawPalettes.push_back(ParsePalette(word));
            }
            else if (kind == "MC")
                fields >> game.Music.Pc >> game.Music.A >> game.Music.X >> game.Music.Y >> game.Music.Psw >> game.Music.Sp;
            else if (kind == "MD" || kind == "MI")
            {
                std::string hex;
                fields >> hex;
                (kind == "MD" ? game.Music.Dsp : game.Music.Io) = ParseBytes(hex);
            }
            else if (kind == "ME" || kind == "MB")
            {
                KnownBlock block;
                fields >> block.Offset >> block.Length >> block.Dest;
                if (kind == "ME")
                    game.Music.Engine.push_back(block);
                else if (!game.Music.Banks.empty())
                    game.Music.Banks.back().Blocks.push_back(block);
            }
            else if (kind == "MP")
            {
                int dest = 0;
                std::string hex;
                fields >> dest >> hex;
                game.Music.Patches.emplace_back(dest, ParseBytes(hex));
            }
            else if (kind == "MK")
                game.Music.Banks.emplace_back();
            else if (kind == "MS")
            {
                std::array<int, 4> v{};
                fields >> v[0] >> v[1] >> v[2] >> v[3];
                if (!game.Music.Banks.empty())
                    game.Music.Banks.back().Songs.push_back({static_cast<uint8_t>(v[0]), static_cast<uint8_t>(v[1]), static_cast<uint8_t>(v[2]), static_cast<uint8_t>(v[3])});
            }
            else if (kind == "R")
            {
                KnownRun run;
                std::string word;
                fields >> run.Offset >> run.Length >> run.Bpp;
                while (fields >> word)
                    run.Palettes.push_back(ParsePalette(word));
                game.Runs.push_back(run);
            }
        }
        return true;
    }
    return false;
}
} // namespace Dud
