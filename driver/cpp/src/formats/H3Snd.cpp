// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Heroes of Might and Magic 3 .SND support (ported from drv_default.dpr of
// Dragon UnPACKer 5). No signature: number of entries (32 bits) then entries of
// 48 bytes: "name\0extension\0" in 40 bytes, offset, size. The table is
// validated to recognize the format, so the extension is needed.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 10000;

class H3SndArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 4)
            return false;
        const uint32_t count = stream.U32();
        const int64_t tableEnd = 4 + static_cast<int64_t>(count) * 48;
        if (count == 0 || count > MaxEntries || tableEnd > stream.Size())
            return false;
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[40];
            stream.ReadBuffer(name);
            const uint32_t offset = stream.U32();
            const uint32_t size = stream.U32();
            if (name[0] == '\0' || offset < tableEnd || static_cast<int64_t>(offset) + size > stream.Size())
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint32_t count = _stream->U32();
        CheckTable(4, count, 48);

        _formatId = "H3SND";
        _dirSeparator = "";
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[40];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            // The extension is stored after the name's null
            const std::string base = ZString(name);
            const size_t extStart = base.size() + 1;
            std::string ext = extStart < sizeof name ? ZString(name + extStart, sizeof name - extStart) : "";
            // (the files of the GOG release keep garbage after the three letters: ".WAVV", ".WAVVVWAV"...)
            if (ext.size() > 3)
                ext.resize(3);
            Add(base + "." + ext, offset, size);
        }
    }
};
} // namespace

void RegisterH3Snd()
{
    Register<H3SndArchive>("*.SND", "Heroes of Might and Magic 3 (*.SND)");
}
} // namespace Dud
