// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Star Wars Starfighter .PAK ("Europa Packfile") support, ported from drv_default.dpr
// of Dragon UnPACKer 5.
// 33 bytes header: "Europa Packfile", 0, unknown, unknown byte, offset of the entries
// block, size of the entries block, number of entries. Each entry: null terminated
// name then offset, size, unknown (integers).

#include "Registry.h"

namespace Dud
{
namespace
{
class EuropaArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, std::string_view("Europa Packfile\0", 16));
    }

    void Parse() override
    {
        _stream->Seek(21);
        const uint32_t blockOffset = _stream->U32();
        const uint32_t blockSize = _stream->U32();
        const uint32_t count = _stream->U32();
        if (static_cast<int64_t>(blockOffset) + blockSize > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid entries block");
        if (count > blockSize / 13)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");

        _formatId = "Europa";
        _dirSeparator = "\\";
        _extractInternal = false;
        const int64_t endPos = static_cast<int64_t>(blockOffset) + blockSize;
        _stream->Seek(blockOffset);
        for (uint32_t i = 1; i <= count; ++i)
        {
            const std::string name = _stream->CString(1024);
            if (_stream->Pos() + 12 > endPos)
                Fail(DUDI7_ERR_NOTFORMAT, "Truncated entries block");
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            const uint32_t unknown = _stream->U32();
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, size, unknown);
        }
    }
};
} // namespace

void RegisterEuropa()
{
    Register<EuropaArchive>("*.PAK", "Europa Packfile (*.PAK)|Star Wars Starfighter (*.PAK)");
}
} // namespace Dud
