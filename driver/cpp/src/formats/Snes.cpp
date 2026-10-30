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

#include "codecs/Brr.h"
#include "codecs/RomGfx.h"
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
        for (const RomStream &s : ScanSnes(image.data(), image.size(), hiRom))
        {
            const bool sized = s.Codec == RomCodec::Lzss || s.Codec == RomCodec::LzssPayload;
            const std::string base = std::string(sized ? "compressed/lzss_" : "compressed/lz_") + Hex(static_cast<uint64_t>(s.Offset), 6);
            AddDecoded(base + ".bin", s.Codec, s.Offset, s.Packed, s.Size, false);
            if (s.Size % 32 == 0)
                AddDecoded(base + "_4bpp.tga", s.Codec, s.Offset, s.Packed, s.Size, true, TileFormat::Snes4bpp);
            if (s.Size % 24 == 0)
                AddDecoded(base + "_3bpp.tga", s.Codec, s.Offset, s.Packed, s.Size, true, TileFormat::Snes3bpp);
            if (s.Size % 32 != 0 && s.Size % 24 != 0 && s.Size % 16 == 0)
                AddDecoded(base + "_2bpp.tga", s.Codec, s.Offset, s.Packed, s.Size, true, TileFormat::Snes2bpp);
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
