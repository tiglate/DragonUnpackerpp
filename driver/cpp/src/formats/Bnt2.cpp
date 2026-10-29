// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Entropia Universe .BNT support, ported from drv_default.dpr of Dragon UnPACKer 5.
// The last 8 bytes are the offset of the directory and "BNT2". The directory is
// the number of entries then, for each: name ended by a 0x0A byte, size, offset, two
// unknown integers.

#include "Registry.h"

namespace Dud
{
namespace
{
class Bnt2Archive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (stream.Size() < 12)
            return false;
        stream.Seek(stream.Size() - 4);
        char id[4];
        return stream.Read(id, 4) == 4 && Equals(id, "BNT2");
    }

    void Parse() override
    {
        _stream->Seek(_stream->Size() - 8);
        const uint32_t dirOffset = _stream->U32();
        if (dirOffset > _stream->Size() - 8)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory offset");
        _stream->Seek(dirOffset);
        const uint32_t count = _stream->U32();
        if (count > (_stream->Size() - dirOffset) / 17)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");

        _formatId = "BNT2";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (uint32_t i = 0; i < count; ++i)
        {
            std::string name;
            char c;
            do
            {
                _stream->ReadBuffer(&c, 1);
                if (c != '\n')
                    name += c;
                if (name.size() > 1024)
                    Fail(DUDI7_ERR_NOTFORMAT, "Name too long");
            } while (c != '\n');
            const uint32_t size = _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t unknown1 = _stream->U32();
            const uint32_t unknown2 = _stream->U32();
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, size, unknown1, unknown2);
        }
    }
};
} // namespace

void RegisterBnt2()
{
    Register<Bnt2Archive>("*.BNT", "Entropia Universe Archive (*.BNT)");
}
} // namespace Dud
