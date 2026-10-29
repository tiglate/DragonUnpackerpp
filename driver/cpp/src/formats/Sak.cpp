// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Postal .SAK support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (10 bytes): "SAK ", version (1), number of entries (16 bits). Then for
// each entry its null terminated name and its offset. There is no size: a file
// ends where the next one (in offset order) starts, the last one at the end of
// the archive.

#include <utility>

#include "Registry.h"

namespace Dud
{
namespace
{
class SakArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "SAK ") && stream.U32() == 1;
    }

    void Parse() override
    {
        _stream->Seek(8);
        const int count = _stream->U16();
        std::vector<std::string> names(static_cast<size_t>(count));
        std::vector<uint32_t> offsets(static_cast<size_t>(count));
        std::vector<int> order(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            names[static_cast<size_t>(i)] = _stream->CString();
            offsets[static_cast<size_t>(i)] = _stream->U32();
            order[static_cast<size_t>(i)] = i;
        }
        // Entries by increasing offset (simple selection sort, as the Pascal driver does: the
        // order of entries with the same offset is part of the output)
        for (int i = 0; i < count - 1; ++i)
        {
            int best = i;
            for (int j = i + 1; j < count; ++j)
            {
                if (offsets[static_cast<size_t>(order[static_cast<size_t>(j)])] <
                    offsets[static_cast<size_t>(order[static_cast<size_t>(best)])])
                    best = j;
            }
            std::swap(order[static_cast<size_t>(i)], order[static_cast<size_t>(best)]);
        }

        _formatId = "SAK";
        _dirSeparator = "/";
        for (int i = 0; i < count; ++i)
        {
            const auto current = static_cast<size_t>(order[static_cast<size_t>(i)]);
            const int64_t endOffset =
                i < count - 1 ? offsets[static_cast<size_t>(order[static_cast<size_t>(i) + 1])] : _stream->Size();
            if (offsets[current] > endOffset || endOffset > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "An entry is outside of the archive");
            Add(names[current], offsets[current], endOffset - offsets[current]);
        }
    }
};
} // namespace

void RegisterSak()
{
    Register<SakArchive>("*.SAK", "Postal (*.SAK)");
}
} // namespace Dud
