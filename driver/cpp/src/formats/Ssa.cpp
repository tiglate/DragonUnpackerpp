// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Empires: Dawn of the Modern World .SSA support, ported from drv_default.dpr of
// Dragon UnPACKer 5.
// "rass", version, unknown, size of the directory, then entries: name (32 bits length +
// chars), offset, end offset, size. A stored file starts either with raw data or with a
// 14 bytes header "PK01", unpacked size, 2 unknown fields, followed by zlib data.
// The listed size is the unpacked one.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t PkHeader = 14;

class SsaArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "rass");
    }

    void Parse() override
    {
        _stream->Seek(12);
        const int64_t dirEnd = static_cast<int64_t>(_stream->U32()) + 12;
        if (dirEnd > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory size");

        _formatId = "SSA";
        _dirSeparator = "\\";
        _extractInternal = true;
        while (_stream->Pos() < dirEnd)
        {
            const std::string name = _stream->LString32(1024);
            const uint32_t offset = _stream->U32();
            _stream->U32(); // end offset
            const uint32_t size = _stream->U32();
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            const int64_t next = _stream->Pos();
            if (size >= PkHeader)
            {
                _stream->Seek(offset);
                char header[4];
                _stream->ReadBuffer(header);
                if (Equals(header, "PK01"))
                {
                    const uint32_t unpacked = _stream->U32();
                    Add(name, offset, unpacked, 1, size);
                    _stream->Seek(next);
                    continue;
                }
            }
            Add(name, offset, size, 0, 0);
            _stream->Seek(next);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == 0)
            return Archive::Extract(index, sink);
        return ExtractZlib(entry.Offset + PkHeader, entry.DataY - PkHeader, entry.Size, sink);
    }
};
} // namespace

void RegisterSsa()
{
    Register<SsaArchive>("*.SSA", "Empires: Dawn of the Modern World Archive (*.SSA)");
}
} // namespace Dud
