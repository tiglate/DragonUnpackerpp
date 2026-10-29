// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Witcher 2 .DZIP support, ported from drv_default.dpr of Dragon UnPACKer 5.
// "DZIP", version 2, number of entries, unknown, offset of the entries (64 bits).
// Entries: name (16 bits length + chars), 32 bytes: unknown, uncompressed size, offset,
// size (all 64 bits). Files are extracted as stored (they can be compressed, the
// uncompressed size is kept in the entry data). Entries outside of the file are skipped.

#include "Registry.h"

namespace Dud
{
namespace
{
class DzipArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        int32_t version = 0;
        return StartsWith(stream, "DZIP") && stream.Read(&version, 4) == 4 && version == 2;
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t count = _stream->U32();
        _stream->U32();
        int64_t entriesOffset = 0;
        _stream->ReadBuffer(&entriesOffset, 8);
        if (entriesOffset < 24 || entriesOffset > _stream->Size() ||
            count > static_cast<uint64_t>(_stream->Size()) / 34)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");

        _formatId = "DZIP";
        _dirSeparator = "\\";
        _extractInternal = false;
        _stream->Seek(entriesOffset);
        for (uint32_t i = 1; i <= count; ++i)
        {
            const uint16_t length = _stream->U16();
            std::string name(length, '\0');
            if (length > 0)
                _stream->ReadBuffer(name.data(), length);
            int64_t ent[4];
            _stream->ReadBuffer(ent, 32);
            if (ent[2] >= 0 && ent[3] >= 0 && ent[2] + ent[3] <= _stream->Size())
                Add(ZString(name.data(), length), ent[2], ent[3], ent[0], ent[1]);
        }
    }
};
} // namespace

void RegisterDzip()
{
    Register<DzipArchive>("*.DZIP", "The Witcher 2 Archive (*.DZIP)");
}
} // namespace Dud
