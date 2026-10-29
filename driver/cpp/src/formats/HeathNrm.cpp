// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Heath: The Unchosen Path .NRM support, ported from the preliminary reader of drv_default.dpr of Dragon
// UnPACKer 5 (disabled there, EXPFORMATS). "PakkaByRCL^DPL2000" (18 bytes), then packets up to the end of the
// file: type (5 bytes), size of the packet, size of the data, name (32 bits length, then characters) and
// the data, which takes what the packet size leaves after the name.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr size_t MaxEntries = 1000000;
constexpr char Id[] = "PakkaByRCL^DPL2000";

class HeathNrmArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[18];
        return stream.Read(id, 18) == 18 && std::memcmp(id, Id, 18) == 0;
    }

    void Parse() override
    {
        const int64_t total = _stream->Size();
        _stream->Seek(18);
        _formatId = "NRM";
        _dirSeparator = "\\";
        _extractInternal = false;
        while (_stream->Pos() < total)
        {
            _stream->Skip(5);
            const int32_t packet = _stream->I32();
            _stream->I32();
            const uint32_t length = _stream->U32();
            if (length > 1024 || _entries.size() >= MaxEntries)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid name");
            std::string raw(length, '\0');
            if (length > 0)
                _stream->ReadBuffer(raw.data(), length);
            const std::string name = ZString(raw.data(), raw.size());
            const int64_t offset = _stream->Pos();
            const int64_t size = static_cast<int64_t>(packet) - static_cast<int64_t>(name.size());
            if (size < 0 || offset + size > total)
                Fail(DUDI7_ERR_NOTFORMAT, "A packet is outside of the archive");
            Add(name, offset, size);
            _stream->Seek(offset + size);
        }
    }
};
} // namespace

void RegisterHeathNrm()
{
    Register<HeathNrmArchive>("*.NRM", "Heath: The Unchosen Path (*.NRM)");
}
} // namespace Dud
