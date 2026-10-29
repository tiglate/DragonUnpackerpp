// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Indiana Jones and the Infernal Machine / Jedi Knight .GOB support ("GOB " +
// version 20), ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (16 bytes): "GOB ", version (20), unknown, number of entries.
// Entries (136 bytes): offset, size, name[128].

#include "Registry.h"

namespace Dud
{
namespace
{
class Indy3dGobArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "GOB ") && stream.U32() == 20;
    }

    void Parse() override
    {
        _stream->Seek(12);
        const uint32_t count = _stream->U32();
        CheckTable(16, count, 136);

        _formatId = "GOB";
        _dirSeparator = "\\";
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            char name[128];
            _stream->ReadBuffer(name);
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterIndy3d()
{
    Register<Indy3dGobArchive>("*.GOB", "Indiana Jones 3D (*.GOB)|Jedi Knight: Dark Forces 2 (*.GOB)");
}
} // namespace Dud
