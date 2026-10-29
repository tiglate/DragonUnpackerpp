// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Painkiller .PAK support, ported from drv_default.dpr of Dragon UnPACKer 5 (the support
// is partly based on the infos of MrMouse).
// Header: 1 byte (0 or 1) and the offset of the index (32 bits). The index is a count then,
// for each entry: name length, the name scrambled (each char is xored with a key that
// starts with length * 2 + 1 + table[length] + entry number (1 based) and grows by 2 for each
// following char, the table being -2, -1, 0, 1, 2 repeated), offset, size, packed size.
// Entries without offset or without size are skipped. A file whose packed size is not its
// size is a zlib stream. No reliable signature: only tried on .PAK files, the whole index
// is validated.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 1000000;

class PkPakArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint8_t id = 0;
        uint32_t offset = 0;
        if (stream.Read(&id, 1) != 1 || id > 1 || stream.Read(&offset, 4) != 4)
            return false;
        return offset >= 5 && static_cast<int64_t>(offset) + 4 <= stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(1);
        const uint32_t offset = _stream->U32();
        if (offset < 5 || static_cast<int64_t>(offset) + 4 > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid index offset");
        _stream->Seek(offset);
        const uint32_t count = _stream->U32();
        if (count > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");

        _formatId = "PKPAK";
        _dirSeparator = "/";
        _extractInternal = true;
        for (uint32_t i = 1; i <= count; ++i)
        {
            const uint32_t nameLength = _stream->U32();
            if (nameLength == 0 || nameLength > 255)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid name length");
            std::vector<uint8_t> crypted(nameLength);
            _stream->ReadBuffer(crypted.data(), nameLength);
            std::string name(nameLength, '\0');
            int key = static_cast<int>((nameLength * 2 + 1 + nameLength % 5 - 2 + i) & 0xFF);
            for (uint32_t y = 0; y < nameLength; ++y)
            {
                const auto c = static_cast<uint8_t>(crypted[y] ^ static_cast<uint8_t>(key));
                if (c < 32)
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid name");
                name[y] = static_cast<char>(c);
                key = (key + 2) & 0xFF;
            }
            const uint32_t fileOffset = _stream->U32();
            const uint32_t fileSize = _stream->U32();
            const uint32_t embedded = _stream->U32();
            if (fileOffset == 0 || fileSize == 0)
                continue;
            if (static_cast<int64_t>(fileOffset) + embedded > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, fileOffset, fileSize, embedded);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == entry.Size)
            return Archive::Extract(index, sink);
        return ExtractZlib(entry.Offset, entry.DataX, entry.Size, sink);
    }
};
} // namespace

void RegisterPkPak()
{
    Register<PkPakArchive>("*.PAK", "Painkiller Archive (*.PAK)");
}
} // namespace Dud
