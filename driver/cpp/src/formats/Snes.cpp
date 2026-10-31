// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Super Nintendo / Super Famicom ROMs (.sfc, .smc, .swc, .fig).
// No signature: the internal header (title, map mode, sizes, checksum and its complement) is
// looked for at the three places where the mapping puts it (LoROM 0x7FC0, HiROM 0xFFC0,
// ExHiROM 0x40FFC0), and the best scoring candidate wins. Dumps from copiers have 512 more
// bytes at the start (the file size is 512 modulo 1024).
// Entries: the ROM without copier header, an info text, the copier header and the palettes found in the ROM
// (see codecs/Palette). The ROM is not drawn
// as tiles: most games compress their graphics, so that would show noise nearly every time.
// The Nintendo LZ streams found in the ROM are unpacked as entries too (raw data and tile
// sheets, see codecs/RomCompress).

#include <zlib.h>

#include <map>
#include <set>

#include "codecs/Brr.h"
#include "codecs/RomGfx.h"
#include "codecs/RomTables.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t CopierHeader = 512;
constexpr int64_t Candidates[] = {0x7FC0, 0xFFC0, 0x40FFC0};
constexpr int HeaderMinimumScore = 4;

struct SnesHeader
{
    int64_t Offset = 0;
    int Score = -1;
    uint8_t Raw[0x40] = {};
};

// Score of the header candidate at offset of the ROM: 4 for a good checksum, 2 for a map mode
// that fits where the header is, 1 for a readable title, 1 for a reset vector in ROM
auto Score(const RomSource &rom, int64_t offset, SnesHeader &out) -> int
{
    if (offset + 0x40 > rom.Size())
        return -1;
    rom.Read(offset, out.Raw, sizeof out.Raw);
    const uint8_t *h = out.Raw;
    int score = 0;
    const unsigned complement = h[0x1C] | (h[0x1D] << 8);
    const unsigned checksum = h[0x1E] | (h[0x1F] << 8);
    if ((complement ^ checksum) == 0xFFFF)
        score += 4;
    const int mode = h[0x15] & 0x0F;
    const bool fits = offset == Candidates[0] ? (mode == 0 || mode == 2 || mode == 3) : (mode == 1 || mode == 5 || mode == 0xA);
    if (fits && (h[0x15] & 0xE0) == 0x20)
        score += 2;
    bool readable = true;
    for (int i = 0; i < 21; ++i)
        readable = readable && ((h[i] >= 0x20 && h[i] < 0x7F) || h[i] >= 0xA0 || h[i] == 0);
    if (readable)
        score += 1;
    if ((h[0x3C] | (h[0x3D] << 8)) >= 0x8000)
        score += 1;
    return score;
}

auto Best(const RomSource &rom, SnesHeader &best) -> bool
{
    for (const int64_t offset : Candidates)
    {
        SnesHeader candidate;
        candidate.Offset = offset;
        candidate.Score = Score(rom, offset, candidate);
        if (candidate.Score > best.Score)
            best = candidate;
    }
    return best.Score >= HeaderMinimumScore;
}

auto CopierBytes(Reader &stream) -> int64_t
{
    return stream.Size() % 1024 == CopierHeader ? CopierHeader : 0;
}

auto MapName(int mode) -> std::string
{
    switch (mode & 0x0F)
    {
    case 0:
        return "LoROM";
    case 1:
        return "HiROM";
    case 2:
        return "LoROM + S-DD1";
    case 3:
        return "LoROM + SA-1";
    case 5:
        return "ExHiROM";
    case 0xA:
        return "HiROM + SPC7110";
    default:
        return "unknown (" + Hex(static_cast<uint64_t>(mode), 2) + ")";
    }
}

// The tile format the tables call bpp (5: 4 bits pixels by rows, the source of Mode 7 tiles)
auto FormatOfBpp(int bpp) -> TileFormat
{
    return bpp == 8 ? TileFormat::Snes8bppLinear : bpp == 5 ? TileFormat::Snes4bppLinear : bpp == 4 ? TileFormat::Snes4bpp
           : bpp == 3 ? TileFormat::Snes3bpp : TileFormat::Snes2bpp;
}

class SnesArchive : public RomArchive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        if (stream.Size() < 0x8000)
            return false;
        SnesHeader best;
        return Best(RomSource(stream, CopierBytes(stream), false), best);
    }

    void Parse() override
    {
        const int64_t copier = CopierBytes(*_stream);
        SetRom(copier, false);
        SnesHeader header;
        if (!Best(*_rom, header))
            Fail(DUDI7_ERR_NOTFORMAT, "No SNES header");
        _formatId = "SNES";
        _dirSeparator = "/";

        const uint8_t *h = header.Raw;
        const unsigned checksum = h[0x1E] | (h[0x1F] << 8);
        const unsigned complement = h[0x1C] | (h[0x1D] << 8);
        std::string info;
        info += InfoLine("Title", HeaderText(h, 21));
        info += InfoLine("Mapping", MapName(h[0x15]));
        info += InfoLine("Speed", (h[0x15] & 0x10) ? "FastROM" : "SlowROM");
        info += InfoLine("Header at", Hex(static_cast<uint64_t>(header.Offset), 6));
        info += InfoLine("Cartridge type", Hex(h[0x16], 2));
        info += InfoLine("ROM size (header)", h[0x17] < 20 ? std::to_string(1 << h[0x17]) + " KB" : "?");
        info += InfoLine("ROM size (file)", std::to_string(_rom->Size() / 1024) + " KB");
        info += InfoLine("SRAM size", h[0x18] > 0 && h[0x18] < 16 ? std::to_string(1 << h[0x18]) + " KB" : "none");
        info += InfoLine("Country", Hex(h[0x19], 2));
        info += InfoLine("Licensee", Hex(h[0x1A], 2));
        info += InfoLine("Version", std::to_string(h[0x1B]));
        info += InfoLine("Checksum", Hex(checksum, 4) + ((complement ^ checksum) == 0xFFFF ? " (complement ok)" : " (complement wrong)"));
        AddText("info.txt", info);

        if (copier != 0)
            AddFileBytes("copier_header.bin", 0, copier);
        AddRaw("ROM.sfc", 0, _rom->Size());

        // Nintendo LZ streams: the data and, when the size fits 2, 3 or 4 bits tiles, the sheets
        std::vector<uint8_t> image(static_cast<size_t>(_rom->Size()));
        _rom->Read(0, image.data(), _rom->Size());
        AddPalettes(image.data(), image.size(), FindSnesPalettes(image.data(), image.size()));
        const bool hiRom = (h[0x15] & 1) != 0;

        // a popular game (data/rom-metadata): the palettes its screens used give the sheets their colours
        KnownGame known;
        const bool haveKnown = FindKnownGame(static_cast<uint32_t>(crc32(0, image.data(), static_cast<uInt>(image.size()))), known);
        std::map<int64_t, const KnownStream *> knownAt;
        std::set<std::vector<uint32_t>> listed;
        const auto listPalette = [&](const std::string &label, const std::vector<uint32_t> &colors) {
            if (colors.empty() || !listed.insert(colors).second)
                return;
            AddColorPalette("palettes/known_" + label, colors);
        };
        if (haveKnown)
        {
            for (const KnownStream &k : known.Streams)
            {
                if (k.Codec != "dkc" && k.Codec != "lz5")
                    knownAt[k.Offset] = &k;
                for (size_t i = 0; i < k.Palettes.size(); ++i)
                    listPalette(Hex(static_cast<uint64_t>(k.Offset), 6) + "_" + std::to_string(i + 1), k.Palettes[i]);
            }
            for (size_t i = 0; i < known.RawPalettes.size(); ++i)
                listPalette("tiles_" + std::to_string(i + 1), known.RawPalettes[i]);
            for (const KnownRun &run : known.Runs)
            {
                const TileFormat format = FormatOfBpp(run.Bpp);
                std::vector<uint32_t> colors;
                if (!run.Palettes.empty())
                    colors = run.Palettes[0];
                else
                    for (const auto &p : known.RawPalettes)
                        if (p.size() == (run.Bpp == 8 ? 256u : run.Bpp == 2 ? 4u : 16u))
                        {
                            colors = p;
                            break;
                        }
                AddColoredSheets("graphics", "tiles" + std::to_string(run.Bpp) + "bpp", run.Offset, run.Length, format,
                                 TileBytes(format) * 256, colors);
            }
        }
        const std::vector<RomStream> scanned = ScanSnes(image.data(), image.size(), hiRom);
        std::set<int64_t> scannedAt;
        for (const RomStream &s : scanned)
            scannedAt.insert(s.Offset);
        // streams the scanner cannot find (no pointer tables to follow, or too many false hits): the table of the game
        // says where they are
        for (const KnownStream &k : known.Streams)
        {
            const RomCodec codec = k.Codec == "dkc" ? RomCodec::DkcWords : RomCodec::Lz5;
            if ((k.Codec != "dkc" && k.Codec != "lz5") || k.Offset < 0 || static_cast<size_t>(k.Offset) >= image.size() ||
                scannedAt.count(k.Offset))
                continue;
            std::vector<uint8_t> data;
            size_t used = 0;
            if (!Decode(codec, image.data() + k.Offset, image.size() - static_cast<size_t>(k.Offset), data, used))
                continue;
            const std::string base = "compressed/" + k.Codec + "_" + Hex(static_cast<uint64_t>(k.Offset), 6);
            const std::vector<uint32_t> colors = k.Palettes.empty() ? std::vector<uint32_t>() : k.Palettes[0];
            const auto size = static_cast<int64_t>(data.size());
            AddDecoded(base + ".bin", codec, k.Offset, static_cast<int64_t>(used), size, false);
            for (const int bpp : k.Bpp)
            {
                const TileFormat format = FormatOfBpp(bpp);
                if (size % TileBytes(format) == 0)
                    AddDecoded(base + "_" + std::to_string(bpp) + "bpp.tga", codec, k.Offset, static_cast<int64_t>(used), size, true, format, colors);
            }
        }
        for (const RomStream &s : scanned)
        {
            std::vector<uint32_t> colors;
            if (const auto it = knownAt.find(s.Offset); it != knownAt.end() && !it->second->Palettes.empty())
                colors = it->second->Palettes[0];
            const bool sized = s.Codec == RomCodec::Lzss || s.Codec == RomCodec::LzssPayload;
            const bool packets = s.Codec == RomCodec::LzPackets;
            const std::string base = std::string(sized ? "compressed/lzss_" : packets ? "compressed/lzp_" : "compressed/lz_") + Hex(static_cast<uint64_t>(s.Offset), 6);
            AddDecoded(base + ".bin", s.Codec, s.Offset, s.Packed, s.Size, false);
            const auto seen = knownAt.find(s.Offset);
            if (haveKnown)
            {
                // a known game: the sheets are the tile formats the screen used from the stream; a stream that was never
                // on screen has no sheets (guessing the format gives noise nearly every time)
                if (seen != knownAt.end())
                    for (const int bpp : seen->second->Bpp)
                    {
                        const TileFormat format = FormatOfBpp(bpp);
                        if (s.Size % TileBytes(format) == 0)
                            AddDecoded(base + "_" + std::to_string(bpp) + (bpp >= 5 ? "bpp_linear.tga" : "bpp.tga"), s.Codec, s.Offset, s.Packed,
                                       s.Size, true, format, colors);
                    }
                continue;
            }
            if (s.Size % 32 == 0)
                AddDecoded(base + "_4bpp.tga", s.Codec, s.Offset, s.Packed, s.Size, true, TileFormat::Snes4bpp, colors);
            if (s.Size % 24 == 0)
                AddDecoded(base + "_3bpp.tga", s.Codec, s.Offset, s.Packed, s.Size, true, TileFormat::Snes3bpp, colors);
            if (s.Size % 32 != 0 && s.Size % 24 != 0 && s.Size % 16 == 0)
                AddDecoded(base + "_2bpp.tga", s.Codec, s.Offset, s.Packed, s.Size, true, TileFormat::Snes2bpp, colors);
        }

        // BRR samples (the games copy them as they are to the sound chip): WAV files
        for (const BrrSample &sample : ScanBrr(image.data(), image.size()))
            AddBrr("audio/brr_" + Hex(static_cast<uint64_t>(sample.Offset), 6) + (sample.Loops ? "_loop" : "") + ".wav",
                   sample.Offset, sample.Blocks);
    }
};
} // namespace

void RegisterSnes()
{
    Register<SnesArchive>("*.SFC;*.SMC;*.SWC;*.FIG", "Super Nintendo / Super Famicom ROM (*.SFC;*.SMC;*.SWC;*.FIG)");
}
} // namespace Dud
