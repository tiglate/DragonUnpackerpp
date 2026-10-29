// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Ascendancy .COB support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Number of entries, then 50 bytes names, then one 32 bits offset per entry (strictly
// increasing). Sizes come from the next offset. There is no signature: only tried on
// .COB files, with a complete check of the table.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t NameSize = 50;

class CobArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        const uint32_t count = stream.U32();
        return count > 0 && count < 65536 && 4 + static_cast<int64_t>(count) * (NameSize + 4) < stream.Size();
    }

    void Parse() override
    {
        const int64_t total = _stream->Size();
        _stream->Seek(0);
        const auto count = static_cast<int32_t>(_stream->U32());
        const int64_t base = 4 + static_cast<int64_t>(count) * (NameSize + 4);
        if (count < 1 || count > 65535 || base >= total)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        std::vector<uint8_t> table(static_cast<size_t>(count) * (NameSize + 4));
        _stream->ReadBuffer(table.data(), table.size());
        std::vector<std::string> names(static_cast<size_t>(count));
        std::vector<uint32_t> offsets(static_cast<size_t>(count));
        for (size_t i = 0; i < names.size(); ++i)
        {
            names[i] = ZString(reinterpret_cast<const char *>(table.data()) + i * NameSize, NameSize);
            if (names[i].empty())
                Fail(DUDI7_ERR_NOTFORMAT, "Empty name");
            for (const char c : names[i])
            {
                if (static_cast<unsigned char>(c) < ' ')
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid name");
            }
            std::memcpy(&offsets[i], table.data() + static_cast<size_t>(count) * NameSize + i * 4, 4);
            if (offsets[i] < base || offsets[i] > total || (i > 0 && offsets[i - 1] >= offsets[i]))
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid offset");
        }

        _formatId = "COB";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (size_t i = 0; i < names.size(); ++i)
        {
            if (i + 1 < names.size())
                Add(names[i], offsets[i], static_cast<int64_t>(offsets[i + 1]) - offsets[i]);
            else
                Add(names[i], offsets[i], total - offsets[i]);
        }
    }
};
} // namespace

void RegisterCob()
{
    Register<CobArchive>("*.COB", "Ascendancy Archive (*.COB)");
}
} // namespace Dud
