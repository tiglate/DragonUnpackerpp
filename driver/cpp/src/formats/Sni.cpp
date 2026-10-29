// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// MDK .SNI support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (24 bytes): the file size - 4 (the only signature, so the extension is
// needed), name of the original file (12 bytes, its extension is given to the
// entries: .SND becomes .WAV), unknown, number of entries.
// Entries (24 bytes): name[12], flag, offset, size. Offsets are 4 bytes short.

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
class SniArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        return stream.Size() >= 24 && static_cast<int64_t>(stream.U32()) == stream.Size() - 4;
    }

    void Parse() override
    {
        _stream->Seek(4);
        char headerName[12];
        _stream->ReadBuffer(headerName);
        _stream->U32();
        const uint32_t count = _stream->U32();
        CheckTable(24, count, 24);
        std::string ext = ToUpper(ExtensionOf(ZString(headerName)));
        if (ext == ".SND")
            ext = ".WAV";

        _formatId = "SNI";
        _dirSeparator = "";
        _stream->Seek(24);
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[12];
            _stream->ReadBuffer(name);
            _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            if (static_cast<int64_t>(offset) + 4 + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(name) + ext, static_cast<int64_t>(offset) + 4, size);
        }
    }
};
} // namespace

void RegisterSni()
{
    Register<SniArchive>("*.SNI", "MDK (*.SNI)");
}
} // namespace Dud
