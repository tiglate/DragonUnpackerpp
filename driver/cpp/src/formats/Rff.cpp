// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Blood .RFF support (Monolith's Resource File Format, versions 2.x and 3.x).
// Header (32 bytes): "RFF\x1A", version, directory offset, number of files.
// Directory: 48 bytes per file (16 bytes cache, offset, size, packed size,
// time, flags, ext[3], name[8], id). From version 3.1 the directory is
// encrypted: xor with a key starting at the low byte of the directory offset
// and incremented every two bytes. Files flagged $10 have their first 256
// bytes encrypted (xor with the byte position div 2).

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t FlagEncrypted = 0x10;
constexpr uint32_t EntrySize = 48;
constexpr int64_t CryptLength = 256;

class RffArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (!StartsWith(stream, std::string_view("RFF\x1A", 4)))
            return false;
        uint32_t version = 0;
        if (stream.Read(&version, 4) != 4)
            return false;
        const uint32_t major = version >> 8;
        return major == 2 || major == 3;
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t version = _stream->U32();
        const uint32_t dirOffset = _stream->U32();
        const uint32_t count = _stream->U32();
        CheckTable(dirOffset, count, EntrySize);

        std::vector<uint8_t> dir(static_cast<size_t>(count) * EntrySize);
        if (!dir.empty())
        {
            _stream->Seek(dirOffset);
            _stream->ReadBuffer(dir.data(), dir.size());
            if (version >= 0x0301)
            {
                uint32_t key = dirOffset << 1;
                for (auto &byte : dir)
                {
                    byte ^= static_cast<uint8_t>(key >> 1);
                    ++key;
                }
            }
        }

        _formatId = "RFF";
        _dirSeparator = "";
        // Blood RFF files hold encrypted entries: always go through Extract
        _extractInternal = true;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint8_t *e = dir.data() + static_cast<size_t>(i) * EntrySize;
            std::string name = ZString(reinterpret_cast<const char *>(e + 36), 8);
            const std::string ext = ZString(reinterpret_cast<const char *>(e + 33), 3);
            if (!ext.empty())
                name += "." + ext;
            uint32_t offset = 0, size = 0, id = 0;
            std::memcpy(&offset, e + 16, 4);
            std::memcpy(&size, e + 20, 4);
            std::memcpy(&id, e + 44, 4);
            // offset, size, flags, id
            Add(name, offset, size, e[32], id);
        }
    }

  protected:
    void DecodeChunk(int64_t index, uint8_t *buffer, int64_t length, int64_t pos) override
    {
        if ((_entries[static_cast<size_t>(index)].DataX & FlagEncrypted) == 0)
            return;
        for (int64_t i = 0; i < length && pos + i < CryptLength; ++i)
            buffer[i] ^= static_cast<uint8_t>((pos + i) >> 1);
    }
};
} // namespace

void RegisterRff()
{
    Register<RffArchive>("*.RFF", "Blood (*.RFF)");
}
} // namespace Dud
