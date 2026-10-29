// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// F-22 Air Dominance Fighter / TAW and EF2000 (DID) .DAT support, ported from drv_default.dpr of Dragon UnPACKer
// 5. The file starts with the offset of the directory (32 bits) and a constant ($50001 for F-22, $2004152 for
// EF2000). The directory: number of entries, number of folder names, number of extension names, size of the names
// block (16 bits each), the names (8 bits length + characters) and the entries (hash, offset, size: 12 bytes).
// The names are lost: the entries are called after their hash, with ".RA" when the data starts with "RA" (a
// compression of the games that is not decoded). There is no signature, so the checks of v5 are all kept and
// only files with the extension .DAT are tried.

#include <cstdio>

#include "Registry.h"

namespace Dud
{
namespace
{
class F22DatArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t h[2];
        if (stream.Size() < 16 || stream.Read(h, 8) != 8)
            return false;
        if (static_cast<int64_t>(h[0]) + 8 >= stream.Size() || (h[1] != 0x50001 && h[1] != 0x2004152) || h[0] <= 4)
            return false;
        stream.Seek(h[0]);
        uint16_t count, names, extNames, nameSize;
        if (stream.Read(&count, 2) != 2 || stream.Read(&names, 2) != 2 || stream.Read(&extNames, 2) != 2 ||
            stream.Read(&nameSize, 2) != 2)
            return false;
        return static_cast<int64_t>(h[0]) + count * 12 + nameSize + 8 <= stream.Size();
    }

    void Parse() override
    {
        const int64_t total = _stream->Size();
        _stream->Seek(0);
        const int32_t dirOffset = _stream->I32();
        if (dirOffset <= 4 || dirOffset > total)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory offset");
        _stream->Seek(dirOffset);
        const uint16_t dirNum = _stream->U16();
        const uint16_t dirNameNum = _stream->U16();
        const uint16_t extNameNum = _stream->U16();
        const uint16_t nameSize = _stream->U16();
        if (dirOffset + static_cast<int64_t>(nameSize) + 8 > total)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid size of the names");
        for (int i = 0; i < dirNameNum + extNameNum; ++i)
        {
            const uint8_t length = _stream->U8();
            _stream->Skip(length);
        }
        if (_stream->Pos() > dirOffset + static_cast<int64_t>(nameSize) + 8)
            Fail(DUDI7_ERR_NOTFORMAT, "The names are longer than announced");
        const int64_t entOffset = dirOffset + static_cast<int64_t>(nameSize) + 8;
        _formatId = "F22DAT";
        _dirSeparator = "";
        _extractInternal = false; // (the RA compression is not decoded)
        for (uint16_t x = 0; x < dirNum; ++x)
        {
            _stream->Seek(entOffset + static_cast<int64_t>(x) * 12);
            const uint32_t hash = _stream->U32();
            const int32_t offset = _stream->I32();
            const int32_t size = _stream->I32();
            if (offset < 0 || offset > total || size < 0 || size > total || static_cast<int64_t>(offset) + size > total)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            char test[2] = {};
            _stream->Seek(offset);
            _stream->Read(test, 2);
            char name[16];
            std::snprintf(name, sizeof name, "%08X", hash);
            Add(test[0] == 'R' && test[1] == 'A' ? std::string(name) + ".RA" : std::string(name), offset, size);
        }
    }
};
} // namespace

void RegisterF22Dat()
{
    Register<F22DatArchive>("*.DAT", "F-22 / EF2000 Archive (*.DAT)");
}
} // namespace Dud
