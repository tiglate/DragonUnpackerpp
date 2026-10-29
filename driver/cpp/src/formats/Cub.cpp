// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Artifex Mundi .CUB support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Everything is XORed with $96. Header (268 bytes): "cub" + null, "1.0" + null,
// number of entries, 256 bytes of description. Entries (264 bytes): name[256],
// offset, size. The data of the files is XORed too (decoded on extraction).

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint8_t Key = 0x96;
constexpr int64_t HeaderSize = 268;
constexpr uint32_t EntrySize = 264;

class CubArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint8_t h[8];
        if (stream.Read(h, 8) != 8)
            return false;
        for (auto &b : h)
            b ^= Key;
        constexpr uint8_t expected[8] = {'c', 'u', 'b', 0, '1', '.', '0', 0};
        return std::memcmp(h, expected, 8) == 0;
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t count = _stream->U32() ^ 0x96969696u;
        CheckTable(HeaderSize, count, EntrySize);

        _formatId = "CUB";
        _dirSeparator = "\\";
        _extractInternal = true;
        std::vector<uint8_t> dir(static_cast<size_t>(count) * EntrySize);
        _stream->Seek(HeaderSize);
        if (!dir.empty())
            _stream->ReadBuffer(dir.data(), dir.size());
        for (auto &b : dir)
            b ^= Key;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint8_t *e = dir.data() + static_cast<size_t>(i) * EntrySize;
            uint32_t offset = 0, size = 0;
            std::memcpy(&offset, e + 256, 4);
            std::memcpy(&size, e + 260, 4);
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(reinterpret_cast<const char *>(e), 256), offset, size);
        }
    }

  protected:
    void DecodeChunk(int64_t, uint8_t *buffer, int64_t length, int64_t) override
    {
        for (int64_t i = 0; i < length; ++i)
            buffer[i] ^= Key;
    }
};
} // namespace

void RegisterCub()
{
    Register<CubArchive>("*.CUB", "Artifex Mundi Games (*.CUB)|Nightmares from the Deep: The Cursed Heart "
                                  "(*.CUB)|Nightmares from the Deep 2: The Siren's Call (*.CUB)|Nightmares from the "
                                  "Deep 3: Davy Jones (*.CUB)");
}
} // namespace Dud
