// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Lemmings Revolution .BOX support, ported from drv_default.dpr of Dragon UnPACKer 5.
// "LEMBOX", number of entries, directory size, then the names (32 bits length +
// chars) and one offset per entry. Sizes come from the next offset (the last file
// ends at the end of the archive).

#include "Registry.h"

namespace Dud
{
namespace
{
class LemBoxArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "LEMBOX");
    }

    void Parse() override
    {
        const int64_t total = _stream->Size();
        _stream->Seek(6);
        const int32_t count = _stream->I32();
        if (count < 0 || static_cast<int64_t>(count) * 8 > total)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        _stream->Seek(14);
        std::vector<std::string> names(static_cast<size_t>(count));
        std::vector<uint32_t> offsets(static_cast<size_t>(count));
        for (auto &name : names)
            name = _stream->LString32(1024);
        for (auto &offset : offsets)
            offset = _stream->U32();

        _formatId = "BOX";
        _dirSeparator = "\\";
        _extractInternal = false;
        int64_t last = 0;
        for (size_t i = 0; i < offsets.size(); ++i)
        {
            if (offsets[i] > total || offsets[i] < last)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid offset");
            last = offsets[i];
            if (i + 1 < offsets.size())
            {
                if (offsets[i + 1] < offsets[i])
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid offset");
                Add(names[i], offsets[i], static_cast<int64_t>(offsets[i + 1]) - offsets[i]);
            }
            else
            {
                Add(names[i], offsets[i], total - offsets[i]);
            }
        }
    }
};
} // namespace

void RegisterLemBox()
{
    Register<LemBoxArchive>("*.BOX", "Lemmings Revolution Archive (*.BOX)");
}
} // namespace Dud
