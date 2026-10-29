// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Duke Nukem 3D / Shadow Warrior .GRP support
// (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header: "KenSilverman", number of files. Then the entries (name[12], size);
// the data follows the directory, one file after the other.

#include "Registry.h"

namespace Dud
{
namespace
{
class GrpArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "KenSilverman");
    }

    void Parse() override
    {
        _stream->Seek(12);
        const uint32_t count = _stream->U32();
        CheckTable(16, count, 16);

        _formatId = "KSGRP";
        _dirSeparator = "";
        int64_t offset = 16 + static_cast<int64_t>(count) * 16;
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[12];
            _stream->ReadBuffer(name);
            const uint32_t size = _stream->U32();
            Add(ZString(name), offset, size);
            offset += size;
        }
    }
};
} // namespace

void RegisterGrp()
{
    Register<GrpArchive>("*.GRP", "Duke Nukem 3D (*.GRP)|Shadow Warrior (*.GRP)");
}
} // namespace Dud
