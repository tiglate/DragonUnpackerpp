// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Fable: The Lost Chapters .BIG (BIGB) support, ported from drv_default.dpr of
// Dragon UnPACKer 5 (marked experimental there too: many fields are unknown and skipped).
// Header: "BIGB", unknown, footer offset. The footer is a list of banks: name (null
// terminated), 20 bytes (unknown, number of entries, offset of the entries, 2 unknown).
// At the offset of a bank: a count of 8 bytes records to skip, then the entries: 24 bytes
// (unknown, counter, unknown, size, offset, checksum), name (32 bits length + chars),
// 2 integers (the second one is a count of blocks of size + data to skip) and a last
// block of size + data to skip.

#include "Registry.h"

namespace Dud
{
namespace
{
class FableBigArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (!StartsWith(stream, "BIGB"))
            return false;
        stream.U32();
        return stream.U32() < stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t footer = _stream->U32();
        if (footer >= _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid footer offset");
        _stream->Seek(footer);
        const uint32_t bankCount = _stream->U32();
        if (bankCount > static_cast<uint64_t>(_stream->Size()) / 21)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of banks");

        _formatId = "BIGB";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (uint32_t i = 1; i <= bankCount; ++i)
        {
            const std::string base = _stream->CString(1024);
            uint32_t ftr[5];
            _stream->ReadBuffer(ftr, 20);
            const uint32_t count = ftr[1];
            const int64_t current = _stream->Pos();
            if (ftr[2] >= _stream->Size() || count > static_cast<uint64_t>(_stream->Size()) / 28)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid bank");
            _stream->Seek(ftr[2]);
            const uint32_t unknownCount = _stream->U32();
            if (_stream->Pos() + static_cast<int64_t>(unknownCount) * 8 > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid bank");
            _stream->Skip(static_cast<int64_t>(unknownCount) * 8);
            for (uint32_t j = 1; j <= count; ++j)
            {
                uint32_t ent[6];
                _stream->ReadBuffer(ent, 24);
                const std::string display = _stream->LString32(1024);
                _stream->U32();
                const uint32_t descriptions = _stream->U32();
                if (descriptions > static_cast<uint64_t>(_stream->Size()) / 4)
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid description count");
                for (uint32_t k = 1; k <= descriptions; ++k)
                    SkipBlock();
                SkipBlock();
                if (static_cast<int64_t>(ent[4]) + ent[3] > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                Add(base + "\\" + display, ent[4], ent[3], ent[1], ent[5]);
            }
            _stream->Seek(current);
        }
    }

  private:
    void SkipBlock()
    {
        const uint32_t length = _stream->U32();
        if (_stream->Pos() + length > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid block size");
        _stream->Skip(length);
    }
};
} // namespace

void RegisterFableBig()
{
    Register<FableBigArchive>("*.BIG", "Fable: The Lost Chapters Archive (*.BIG)");
}
} // namespace Dud
