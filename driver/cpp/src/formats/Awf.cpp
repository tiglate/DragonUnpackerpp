// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Qui veut gagner des millions (Who wants to be a millionaire) .AWF support
// (ported from drv_default.dpr of Dragon UnPACKer 5). No signature: number of
// entries (32 bits) then entries of 264 bytes (offset, name[260]). A file ends
// where the next one starts, the last one at the end of the archive. The table
// is validated, so the extension is needed.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 100000;

class AwfArchive : public Archive
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
        const int64_t tableEnd = 4 + static_cast<int64_t>(count) * 264;
        if (count == 0 || count > MaxEntries || tableEnd > stream.Size())
            return false;
        uint32_t previous = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = stream.U32();
            char name[260];
            stream.ReadBuffer(name);
            // Files in order, after the table, inside the archive
            if (name[0] == '\0' || offset < tableEnd || offset < previous || offset > stream.Size())
                return false;
            previous = offset;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint32_t count = _stream->U32();
        CheckTable(4, count, 264);

        _formatId = "AWF";
        _dirSeparator = "";
        uint32_t oldOffset = 0;
        std::string oldName;
        for (uint32_t i = 1; i <= count; ++i)
        {
            const uint32_t offset = _stream->U32();
            char name[260];
            _stream->ReadBuffer(name);
            if (i > 1)
                Add(oldName, oldOffset, static_cast<int64_t>(offset) - oldOffset);
            oldName = ZString(name);
            oldOffset = offset;
        }
        // The last one goes up to the end of the file
        Add(oldName, oldOffset, _stream->Size() - oldOffset);
    }
};
} // namespace

void RegisterAwf()
{
    Register<AwfArchive>("*.AWF", "Qui veut gagner des millions (*.AWF)|Who wants to be a millionaire (*.AWF)");
}
} // namespace Dud
