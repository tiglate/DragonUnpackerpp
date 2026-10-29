// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Jagged Alliance 2 .SLF support (ported from drv_default.dpr of Dragon
// UnPACKer 5). Header (532 bytes): library name[256], directory[256], number of
// entries, number of entries again, then the signature FF FF 00 02 01 00 00 00
// 00 00 00 00. The entries (280 bytes: name[256], offset, size, 16 bytes) are at
// the end of the file. Names are prefixed with the directory.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
class SlfArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(520);
        // (the fifth byte is 1 in most files and 0 in others, such as the Data.slf of Jagged Alliance 2 on GOG)
        constexpr uint8_t expected[12] = {0xFF, 0xFF, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0};
        uint8_t id[12];
        if (stream.Size() < 532 || stream.Read(id, 12) != 12)
            return false;
        if (id[4] > 1)
            return false;
        id[4] = expected[4];
        return std::memcmp(id, expected, 12) == 0;
    }

    void Parse() override
    {
        _stream->Seek(256);
        char dirName[256];
        _stream->ReadBuffer(dirName);
        const uint32_t count = _stream->U32();
        const int64_t dirOffset = _stream->Size() - static_cast<int64_t>(count) * 280;
        if (dirOffset < 532)
            Fail(DUDI7_ERR_NOTFORMAT, "The directory is outside of the file");

        _formatId = "SLF";
        _dirSeparator = "\\";
        _stream->Seek(dirOffset);
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[256];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            _stream->Skip(16);
            if (static_cast<int64_t>(offset) + size > dirOffset)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(dirName) + ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterSlf()
{
    Register<SlfArchive>("*.SLF", "Jagged Alliance 2 (*.SLF)");
}
} // namespace Dud
