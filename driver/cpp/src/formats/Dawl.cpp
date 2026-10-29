// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Geometry Wars 3: Dimensions Evolved .WAD ("DAWL") support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (12 bytes): "DAWL", major and minor version (1, 1). At 2048: a table of N 8 bytes records, then at
// 2048 + 4 + N * 8 the number of entries and the entries (48 bytes: 2 unknown integers, type, an unknown
// integer, offset, size, size again, 0; 64 bits values). The entries have no name: it is made of the type and
// the unknown values, with a folder and an extension guessed from the type and the first bytes of the data.
// Deviation from v5: its header test combined its conditions with "and" (a file was refused only if everything
// was wrong); here the signature must be right, the file at least 2048 bytes and the entries inside of it.

#include <cstdio>
#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t TableStart = 2048;

auto Hex(uint64_t value, int digits) -> std::string
{
    char text[32];
    std::snprintf(text, sizeof text, "%0*llX", digits, static_cast<unsigned long long>(value));
    return text;
}

class DawlArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "DAWL") && stream.Size() >= TableStart;
    }

    void Parse() override
    {
        const int64_t total = _stream->Size();
        _stream->Seek(4);
        const uint32_t major = _stream->U32();
        const uint32_t minor = _stream->U32();
        if (total < TableStart || major != 1 || minor != 1)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");
        _stream->Seek(TableStart);
        const int32_t numI = _stream->I32();
        if (numI < 0 || static_cast<int64_t>(numI) * 8 + 4 + TableStart > total)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid table");
        _stream->Seek(TableStart + 4 + static_cast<int64_t>(numI) * 8);
        const int32_t numE = _stream->I32();
        if (numE < 0 || (total - _stream->Pos()) / 48 < numE)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        std::vector<uint8_t> table(static_cast<size_t>(numE) * 48);
        if (!table.empty())
            _stream->ReadBuffer(table.data(), table.size());

        _formatId = "DAWL";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (int32_t x = 0; x < numE; ++x)
        {
            const uint8_t *p = &table[static_cast<size_t>(x) * 48];
            uint32_t unknown1, unknown2, type, unknown3;
            int64_t offset, size;
            std::memcpy(&unknown1, p, 4);
            std::memcpy(&unknown2, p + 4, 4);
            std::memcpy(&type, p + 8, 4);
            std::memcpy(&unknown3, p + 12, 4);
            std::memcpy(&offset, p + 16, 8);
            std::memcpy(&size, p + 24, 8);
            if (offset < 0 || size < 0 || offset + size > total)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            std::string disp = std::to_string(type) + "-" + Hex(unknown1, 8) + "-" + Hex(unknown2, 8) + "-" +
                               Hex(unknown3, 8);
            switch (type)
            {
            case 2:
                disp = "XML\\SurfaceMesh\\" + disp + ".xml";
                break;
            case 3: {
                if (Word(offset) == 4)
                {
                    std::string name = LengthString(offset + 4);
                    for (char &c : name)
                        if (c == '/')
                            c = '\\';
                    disp = name + ".model";
                }
                else
                    disp = "Entities\\" + disp + ".tga";
                break;
            }
            case 4:
                disp = "Textures\\" + std::to_string(type) + "\\" + disp + ".tga";
                break;
            case 16:
                disp = "XML\\LineMesh\\" + disp + ".xml";
                break;
            case 20: {
                const uint32_t test = Word(offset);
                disp = "Textures\\Fonts\\" + disp;
                if (test == 0x474E5089)
                    disp += ".png";
                else if (test == 0x6F666E69)
                    disp += ".txt";
                else
                    disp += ".20";
                break;
            }
            case 30:
                disp = "XML\\" + disp + ".xml";
                break;
            case 31:
            case 42:
            case 50:
                disp += ".csv";
                break;
            case 43:
            case 44:
                disp += ".kslf";
                break;
            case 49: {
                const uint32_t test = Word(offset);
                disp = "Sounds\\" + disp;
                if (test == 0x46464952)
                    disp += ".fev";
                else if (test == 0x35425346)
                    disp += ".fsb";
                else
                    disp += ".49";
                break;
            }
            default:
                if (type >= 5 && type <= 15)
                {
                    if (Word(offset) == 0x61754C1B)
                    {
                        disp = Strip0(LengthString(offset + 4 + 8));
                        if (disp.size() > 37 && disp.compare(0, 29, "@..\\..\\Image\\DontShip\\Common\\") == 0)
                            disp = disp.substr(disp.size() - (disp.size() - 29));
                        for (size_t at; (at = disp.find("\\\\")) != std::string::npos;)
                            disp.erase(at, 1);
                        disp += "c";
                    }
                    else
                        disp = "Entities\\" + disp + "." + std::to_string(type);
                }
                else if ((type >= 17 && type <= 19) || (type >= 21 && type <= 29) || (type >= 34 && type <= 41) ||
                         (type >= 46 && type <= 48))
                    disp = "Textures\\" + std::to_string(type) + "\\" + disp + ".png";
                else
                    disp += "." + Hex(type, 3);
                break;
            }
            Add(disp, offset, size);
        }
    }

  private:
    // the 32 bits at offset (0 when they are not in the file)
    auto Word(int64_t offset) -> uint32_t
    {
        if (offset < 0 || offset + 4 > _stream->Size())
            return 0;
        _stream->Seek(offset);
        return _stream->U32();
    }

    // a text with a 32 bits length in front (Get32 of v5: the length must be at most 255)
    auto LengthString(int64_t offset) -> std::string
    {
        const uint32_t length = Word(offset);
        if (length > 255 || offset + 4 + length > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid name");
        std::string text(length, '\0');
        if (length > 0)
            _stream->ReadBuffer(text.data(), length);
        return ZString(text.data(), length);
    }

    static auto Strip0(const std::string &text) -> std::string
    {
        return text.substr(0, text.find('\0'));
    }
};
} // namespace

void RegisterDawl()
{
    Register<DawlArchive>("*.WAD", "Geometry Wars 3 Archive (*.WAD)");
}
} // namespace Dud
