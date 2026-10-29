// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Ascaron .CPR support (Port Royale), ported from drv_default.dpr of Dragon UnPACKer 5.
// 32 bytes "ASCARON_ARCHIVE V0.9", then chained blocks: header (sub id, size, number of
// entries, relative offset of the next block) followed by entries (offset, size, unknown,
// null-terminated name). The last block points to itself.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr std::string_view Id = "ASCARON_ARCHIVE V0.9";
constexpr int64_t Start = 32;
constexpr int MaxBlocks = 100000;

class CprArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char buffer[Start];
        return stream.Read(buffer, Start) == Start && std::string_view(buffer, Id.size()) == Id;
    }

    void Parse() override
    {
        _formatId = "CPR";
        _dirSeparator = "\\";
        _extractInternal = false;
        int64_t blockPos = Start;
        int blocks = 0;
        for (;;)
        {
            if (++blocks > MaxBlocks)
                Fail(DUDI7_ERR_NOTFORMAT, "Too many blocks");
            _stream->Seek(blockPos);
            _stream->I32();
            const int32_t size = _stream->I32();
            const int32_t count = _stream->I32();
            const int32_t nextRel = _stream->I32();
            if (count < 0 || count > (_stream->Size() - _stream->Pos()) / 13)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
            int64_t smallest = static_cast<int64_t>(nextRel) + size + _stream->Pos();
            for (int32_t i = 0; i < count; ++i)
            {
                const uint32_t offset = _stream->U32();
                const uint32_t fileSize = _stream->U32();
                const uint32_t unknown = _stream->U32();
                const std::string display = _stream->CString(1024);
                if (offset < smallest)
                    smallest = offset;
                if (!display.empty())
                {
                    if (static_cast<int64_t>(offset) + fileSize > _stream->Size())
                        Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                    Add(display, offset, fileSize, unknown);
                }
            }
            const int64_t nextPos = smallest + nextRel;
            if (nextPos < 0 || nextPos >= _stream->Size())
                break;
            if (nextPos == blockPos)
                break;
            blockPos = nextPos;
        }
    }
};
} // namespace

void RegisterCpr()
{
    Register<CprArchive>("*.CPR", "Ascaron Archive (*.CPR)|Port Royale (*.CPR)");
}
} // namespace Dud
