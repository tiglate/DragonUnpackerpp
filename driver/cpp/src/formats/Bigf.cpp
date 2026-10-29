// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Electronic Arts .BIG support (BIGF/BIG4): Command & Conquer: Generals,
// The Lord of the Rings: Battle for Middle Earth, ..
// (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (16 bytes): "BIGF" or "BIG4", total file size (little-endian),
// number of files and offset of the data (both BIG-endian).
// Entries: offset and size (big-endian) followed by the null terminated name.
// The Fable "BIGB" variant is not supported.

#include "Registry.h"

namespace Dud
{
namespace
{
class BigfArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[4];
        return stream.Read(id, 4) == 4 && (Equals(id, "BIGF") || Equals(id, "BIG4")) &&
               stream.U32() == stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t count = _stream->BE32();
        _stream->BE32(); // offset of the data
        CheckTable(16, count, 9);

        _formatId = "BIGF";
        bool slash = false;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->BE32();
            const uint32_t size = _stream->BE32();
            const std::string name = _stream->CString();
            if (name.find('/') != std::string::npos)
                slash = true;
            Add(name, offset, size);
        }
        _dirSeparator = slash ? "/" : "\\";
    }
};
} // namespace

void RegisterBigf()
{
    Register<BigfArchive>("*.BIG", "Command & Conquer: Generals (*.BIG)|Command & Conquer: Generals - Zero Hour "
                                   "(*.BIG)|The Lord of the Rings: Battle for Middle Earth (*.BIG)|Command & "
                                   "Conquer: Red Alert 3 (*.BIG)");
}
} // namespace Dud
