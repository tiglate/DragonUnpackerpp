// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Hitman 2 / Hitman: Contracts / Freedom Fighters .PRM support (ported from
// drv_default.dpr of Dragon UnPACKer 5). Header (16 bytes): $71E, offset of the
// table of offsets, a second offset, number of entries. The table holds the
// offset of every file (32 bits each): a file ends where the next starts, the
// last one where the table starts. Files have no name: they are numbered after the
// archive. The signature is weak, so the extension is needed.

#include <algorithm>

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
class PrmArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 16 || stream.U32() != 0x71E)
            return false;
        const uint32_t dirOffset = stream.U32();
        const uint32_t dirOffset2 = stream.U32();
        return dirOffset < stream.Size() && dirOffset2 < stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t dirOffset = _stream->U32();
        _stream->U32();
        const uint32_t count = _stream->U32();
        CheckTable(dirOffset, count, 4);
        std::vector<uint32_t> offsets(static_cast<size_t>(count) + 1);
        _stream->Seek(dirOffset);
        for (uint32_t i = 0; i < count; ++i)
            offsets[i] = _stream->U32();
        offsets[count] = dirOffset;
        std::string prefix = FileNameOf(_stream->Path());
        std::ranges::replace(prefix, '.', '_');
        prefix += "_";

        _formatId = "HMCPRM";
        _dirSeparator = "";
        for (uint32_t i = 0; i < count; ++i)
        {
            if (offsets[i + 1] < offsets[i] || static_cast<int64_t>(offsets[i + 1]) > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid file offsets");
            std::string number = std::to_string(static_cast<int64_t>(i) + 1);
            if (number.size() < 8)
                number.insert(0, 8 - number.size(), '0');
            Add(prefix + number + ".raw", offsets[i], offsets[i + 1] - offsets[i]);
        }
    }
};
} // namespace

void RegisterPrm()
{
    Register<PrmArchive>("*.PRM", "Freedom Fighters (*.PRM)|Hitman 2: Silent Assassin (*.PRM)|Hitman: Contracts "
                                  "(*.PRM)");
}
} // namespace Dud
