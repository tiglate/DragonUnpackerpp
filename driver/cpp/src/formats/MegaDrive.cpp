// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Sega Mega Drive / Genesis ROMs (.bin, .md, .gen, .smd).
// The header at 0x100 starts with "SEGA" (console name, copyright, titles, serial number,
// checksum, ROM and RAM ranges, region). .smd files are Super Magic Drive dumps: a 512 bytes
// header and 16 KB blocks with the odd and even bytes stored apart, deinterleaved here.
// Entries: the ROM image and an info text. The ROM is not drawn as tiles: most games compress their
// graphics, so that would show noise nearly every time. The Nemesis and Kosinski streams found in the ROM
// are unpacked as entries too (raw data and tile sheet, see codecs/RomCompress).

#include "codecs/RomGfx.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t HeaderOffset = 0x100;

// Is it a Super Magic Drive dump (header with AA BB at 8 and 9, whole blocks)?
auto LooksInterleaved(Reader &stream) -> bool
{
    uint8_t raw[16];
    stream.Seek(0);
    if (stream.Size() < RomSource::SmdHeader + RomSource::SmdBlock || stream.Read(raw, sizeof raw) != sizeof raw)
        return false;
    return raw[8] == 0xAA && raw[9] == 0xBB && (stream.Size() - RomSource::SmdHeader) % RomSource::SmdBlock == 0;
}

auto HasSegaHeader(const RomSource &rom) -> bool
{
    uint8_t text[16];
    rom.Read(HeaderOffset, text, sizeof text);
    // "SEGA MEGA DRIVE", "SEGA GENESIS", "SEGA 32X", "SEGA SSF", sometimes with a blank before
    const int start = text[0] == ' ' ? 1 : 0;
    return text[start] == 'S' && text[start + 1] == 'E' && text[start + 2] == 'G' && text[start + 3] == 'A';
}

auto Be32(const uint8_t *p) -> uint32_t
{
    return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
}

class MegaDriveArchive : public RomArchive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (stream.Size() < HeaderOffset + 0x100)
            return false;
        const bool smd = LooksInterleaved(stream);
        return HasSegaHeader(RomSource(stream, smd ? RomSource::SmdHeader : 0, smd));
    }

    void Parse() override
    {
        const bool smd = LooksInterleaved(*_stream);
        SetRom(smd ? RomSource::SmdHeader : 0, smd);
        if (_rom->Size() < HeaderOffset + 0x100 || !HasSegaHeader(*_rom))
            Fail(DUDI7_ERR_NOTFORMAT, "No Mega Drive header");
        _formatId = smd ? "MDSMD" : "MD";
        _dirSeparator = "/";

        uint8_t h[0x100];
        _rom->Read(HeaderOffset, h, sizeof h);
        // offsets relative to 0x100
        std::string info;
        info += InfoLine("System", HeaderText(h, 16));
        info += InfoLine("Copyright", HeaderText(h + 0x10, 16));
        info += InfoLine("Domestic name", HeaderText(h + 0x20, 48));
        info += InfoLine("Overseas name", HeaderText(h + 0x50, 48));
        info += InfoLine("Serial number", HeaderText(h + 0x80, 14));
        info += InfoLine("Checksum", Hex((uint32_t{h[0x8E]} << 8) | h[0x8F], 4));
        info += InfoLine("Devices", HeaderText(h + 0x90, 16));
        info += InfoLine("ROM range", Hex(Be32(h + 0xA0), 6) + "-" + Hex(Be32(h + 0xA4), 6));
        info += InfoLine("RAM range", Hex(Be32(h + 0xA8), 6) + "-" + Hex(Be32(h + 0xAC), 6));
        info += InfoLine("Region", HeaderText(h + 0xF0, 3));
        info += InfoLine("File layout", smd ? "Super Magic Drive (deinterleaved)" : "plain");
        AddText("info.txt", info);

        const int64_t size = _rom->Size();
        AddRaw("ROM.bin", 0, size);

        // Nemesis and Kosinski streams: the data and, drawn as tiles, the art
        std::vector<uint8_t> image(static_cast<size_t>(size));
        _rom->Read(0, image.data(), size);
        for (const RomStream &s : ScanMegaDrive(image.data(), image.size()))
        {
            const bool nemesis = s.Codec == RomCodec::Nemesis;
            const std::string base = std::string(nemesis ? "nemesis/art_" : "kosinski/data_") + Hex(static_cast<uint64_t>(s.Offset), 6);
            AddDecoded(base + ".tga", s.Codec, s.Offset, s.Packed, s.Size, true);
            AddDecoded(base + ".bin", s.Codec, s.Offset, s.Packed, s.Size, false);
        }
    }
};
} // namespace

void RegisterMegaDrive()
{
    Register<MegaDriveArchive>("*.BIN;*.MD;*.GEN;*.SMD", "Sega Mega Drive / Genesis ROM (*.BIN;*.MD;*.GEN;*.SMD)");
}
} // namespace Dud
