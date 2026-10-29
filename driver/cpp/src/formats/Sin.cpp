// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// SiN .SIN support ("SPAK"), ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (12 bytes): "SPAK", directory offset, directory length. Entries are
// 128 bytes: name[120], offset, size.
// The directory length is a size in bytes (like Quake PAK files) but Dragon
// UnPACKer 5 read it as a number of entries: both are accepted, the one that
// fits the file is used (a directory is normally at the end of the file).

#include "Registry.h"

namespace Dud
{
namespace
{
class SinArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "SPAK");
    }

    void Parse() override
    {
        _stream->Seek(4);
        const int64_t dirOffset = _stream->U32();
        const int64_t dirLength = _stream->U32();
        // A length in bytes is a multiple of the entry size and ends the file (or is
        // not a possible number of entries)
        int64_t count = dirLength;
        if (dirLength % 128 == 0 && dirOffset + dirLength <= _stream->Size() &&
            (dirOffset + dirLength == _stream->Size() || dirOffset + dirLength * 128 > _stream->Size()))
            count = dirLength / 128;
        CheckTable(dirOffset, count, 128);

        _formatId = "SIN";
        _dirSeparator = "/";
        _stream->Seek(dirOffset);
        for (int64_t i = 0; i < count; ++i)
        {
            char name[120];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterSin()
{
    Register<SinArchive>("*.SIN", "Sin (*.SIN)");
}
} // namespace Dud
