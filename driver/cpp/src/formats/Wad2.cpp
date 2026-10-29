// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Quake .WAD ("WAD2") and Half-Life .WAD ("WAD3") support
// (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header: ID, number of entries, directory offset. Entries are 32 bytes.

#include <cstdio>

#include "Registry.h"

namespace Dud
{
namespace
{
class Wad2Archive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[4];
        return stream.Read(id, 4) == 4 && (Equals(id, "WAD2") || Equals(id, "WAD3"));
    }

    void Parse() override
    {
        _stream->Seek(0);
        char id[4];
        _stream->ReadBuffer(id);
        const uint32_t count = _stream->U32();
        const uint32_t dirOffset = _stream->U32();
        CheckTable(dirOffset, count, 32);

        _formatId = ZString(id);
        _dirSeparator = "";
        _stream->Seek(dirOffset);
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->U32();
            const uint32_t diskSize = _stream->U32();
            _stream->U32(); // size once unpacked
            const uint8_t type = _stream->U8();
            const uint8_t compression = _stream->U8();
            _stream->U16();
            char name[16];
            _stream->ReadBuffer(name);
            std::string ext;
            switch (type)
            {
            case 64:
                ext = "pal";
                break;
            case 66:
                ext = "psb";
                break;
            case 67:
                ext = "pmm";
                break;
            case 68:
                ext = "mm";
                break;
            case 69:
                ext = "cp";
                break;
            case 70:
                ext = "pcp";
                break;
            default:
                char hex[8];
                std::snprintf(hex, sizeof hex, "%02X", type);
                ext = std::string("unk") + hex;
            }
            // The size on disk is the one to copy; the uncompressed one is not stored
            Add(ZString(name) + "." + ext, offset, diskSize, type, compression);
        }
    }
};
} // namespace

void RegisterWad2()
{
    Register<Wad2Archive>("*.WAD", "Quake (*.WAD)|Half-Life (*.WAD)");
}
} // namespace Dud
