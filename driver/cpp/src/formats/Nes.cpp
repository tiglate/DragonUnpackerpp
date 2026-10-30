// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Nintendo Entertainment System ROMs in the iNES / NES 2.0 format (.nes).
// 16 bytes header ("NES" 1A, PRG size in 16 KB units, CHR size in 8 KB units, flags), an
// optional 512 bytes trainer, then the PRG ROM (code and data) and the CHR ROM (tile graphics).
// Entries: the raw parts, an info text and the CHR ROM drawn as sheets of 8 KB (2 bits tiles,
// in gray: the palettes are set by the program). Games without CHR ROM keep their graphics in
// the PRG ROM, copied to the CHR RAM at run time: the PRG ROM is drawn as sheets then.

#include "codecs/RomGfx.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t HeaderSize = 16;
constexpr int64_t TrainerSize = 512;
constexpr int64_t PrgUnit = 16384;
constexpr int64_t ChrUnit = 8192;

struct NesHeader
{
    int64_t Prg = 0;
    int64_t Chr = 0;
    int64_t Trainer = 0;
    int Mapper = 0;
    bool Nes2 = false;
    uint8_t Flags6 = 0;
};

auto ReadHeader(Reader &stream, NesHeader &header) -> bool
{
    uint8_t raw[HeaderSize];
    stream.Seek(0);
    if (stream.Size() < HeaderSize || stream.Read(raw, sizeof raw) != sizeof raw)
        return false;
    if (raw[0] != 'N' || raw[1] != 'E' || raw[2] != 'S' || raw[3] != 0x1A)
        return false;
    header.Nes2 = (raw[7] & 0x0C) == 0x08;
    header.Flags6 = raw[6];
    int64_t prg = raw[4];
    int64_t chr = raw[5];
    header.Mapper = (raw[6] >> 4) | (raw[7] & 0xF0);
    if (header.Nes2)
    {
        if ((raw[9] & 0x0F) == 0x0F || (raw[9] >> 4) == 0x0F)
            return false; // exponent-multiplier sizes: not seen on real games
        prg |= static_cast<int64_t>(raw[9] & 0x0F) << 8;
        chr |= static_cast<int64_t>(raw[9] >> 4) << 8;
        header.Mapper |= (raw[8] & 0x0F) << 8;
    }
    else if (raw[12] != 0 || raw[13] != 0 || raw[14] != 0 || raw[15] != 0)
    {
        // "DiskDude!" and other tags written in the unused bytes of the header: the mapper is
        // then only the low nibble
        header.Mapper = raw[6] >> 4;
    }
    header.Trainer = (raw[6] & 4) ? TrainerSize : 0;
    header.Prg = prg * PrgUnit;
    header.Chr = chr * ChrUnit;
    return prg > 0 && HeaderSize + header.Trainer + header.Prg <= stream.Size();
}

class NesArchive : public RomArchive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        NesHeader header;
        return ReadHeader(stream, header);
    }

    void Parse() override
    {
        NesHeader header;
        if (!ReadHeader(*_stream, header))
            Fail(DUDI7_ERR_NOTFORMAT, "Not an iNES file");
        _formatId = "NES";
        _dirSeparator = "/";
        SetRom(HeaderSize + header.Trainer, false);
        // some dumps are cut short: keep what is there of the CHR ROM
        const int64_t chrStart = HeaderSize + header.Trainer + header.Prg;
        const int64_t chr = std::min(header.Chr, (_stream->Size() - chrStart) / ChrUnit * ChrUnit);

        std::string info;
        info += InfoLine("Format", header.Nes2 ? "NES 2.0" : "iNES");
        info += InfoLine("Mapper", std::to_string(header.Mapper));
        info += InfoLine("PRG ROM", std::to_string(header.Prg / 1024) + " KB");
        info += InfoLine("CHR ROM", chr == 0 ? "none (CHR RAM)" : std::to_string(chr / 1024) + " KB");
        info += InfoLine("Mirroring", (header.Flags6 & 8) ? "four screens" : (header.Flags6 & 1) ? "vertical" : "horizontal");
        info += InfoLine("Battery", (header.Flags6 & 2) ? "yes" : "no");
        AddText("info.txt", info);

        AddFileBytes("header.bin", 0, HeaderSize);
        if (header.Trainer != 0)
            AddFileBytes("trainer.bin", HeaderSize, TrainerSize);
        AddRaw("PRG-ROM.bin", 0, header.Prg);
        if (chr > 0)
        {
            // the CHR ROM follows the PRG ROM in the logical ROM too
            AddRaw("CHR-ROM.bin", header.Prg, chr);
            AddSheets("CHR", "chr", header.Prg, chr, TileFormat::Nes2bpp, ChrUnit);
        }
        else
            AddSheets("PRG", "prg", 0, header.Prg, TileFormat::Nes2bpp, ChrUnit);
    }
};
} // namespace

void RegisterNes()
{
    Register<NesArchive>("*.NES", "Nintendo NES ROM (*.NES)");
}
} // namespace Dud
