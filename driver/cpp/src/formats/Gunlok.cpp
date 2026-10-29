// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Gunlok .DAT support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (20 bytes): "FILECHNK", file size, unknown, number of entries.
// Each entry is a name (32 bits length + text) and 3 values, the last one
// being the position of a chunk (48 bytes header) holding a RIFF wave: the
// entry is the data from 44 bytes in the chunk, for the chunk size - 48.
// Entries whose chunk is not "RIFF" are ignored.

#include "Registry.h"

namespace Dud
{
namespace
{
class GunlokArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "FILECHNK");
    }

    void Parse() override
    {
        _stream->Seek(16);
        const uint32_t count = _stream->U32();
        CheckTable(20, count, 16);

        _formatId = "DAT";
        _dirSeparator = "";
        _stream->Seek(20);
        for (uint32_t i = 0; i < count; ++i)
        {
            const std::string name = _stream->LString32();
            _stream->U32();
            _stream->U32();
            const uint32_t wavePos = _stream->U32();
            const int64_t current = _stream->Pos();
            if (static_cast<int64_t>(wavePos) + 48 <= _stream->Size())
            {
                _stream->Seek(static_cast<int64_t>(wavePos) + 8);
                const uint32_t chunkSize = _stream->U32();
                _stream->Seek(static_cast<int64_t>(wavePos) + 44);
                char chunkType[4];
                _stream->ReadBuffer(chunkType);
                if (Equals(chunkType, "RIFF") && chunkSize >= 48 &&
                    static_cast<int64_t>(wavePos) + chunkSize <= _stream->Size())
                    Add(name, static_cast<int64_t>(wavePos) + 44, static_cast<int64_t>(chunkSize) - 48);
            }
            _stream->Seek(current);
        }
    }
};
} // namespace

void RegisterGunlok()
{
    Register<GunlokArchive>("*.DAT", "Gunlok (*.DAT)");
}
} // namespace Dud
