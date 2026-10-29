// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Civilization IV / V .FPK support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header: version (4 or 6), "FPK_", scrambling flag (1 = names have 1 added to each
// char), a spare byte if version 6, number of entries. Entries: name (32 bits length +
// chars) then 16 bytes (file time, unknown, size, offset). Some files have junk between
// the name and the 16 bytes: a byte giving the number of bytes to skip.

#include "Registry.h"

namespace Dud
{
namespace
{
class FpkArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        int32_t version = 0;
        char id[4];
        return stream.Read(&version, 4) == 4 && stream.Read(id, 4) == 4 && Equals(id, "FPK_") &&
               (version == 4 || version == 6);
    }

    void Parse() override
    {
        const int64_t total = _stream->Size();
        _stream->Seek(0);
        const int32_t version = _stream->I32();
        _stream->Seek(8);
        uint8_t scrambled = 0;
        _stream->ReadBuffer(&scrambled, 1);
        if (version == 6)
            _stream->Skip(1);
        const int32_t count = _stream->I32();
        if (count < 0 || count > total / 20)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");

        _formatId = "FPK" + std::to_string(version);
        _dirSeparator = "\\";
        _extractInternal = false;
        uint32_t ent[4];
        for (int32_t i = 1; i <= count; ++i)
        {
            const int32_t nameLength = _stream->I32();
            if (nameLength < 0 || nameLength > 255)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid file name length");
            std::string name(static_cast<size_t>(nameLength), '\0');
            if (nameLength > 0)
                _stream->ReadBuffer(name.data(), name.size());
            if (scrambled == 1)
            {
                for (auto &c : name)
                    c = static_cast<char>(static_cast<uint8_t>(c) - 1);
            }
            if (const auto nul = name.find('\0'); nul != std::string::npos)
                name.resize(nul);
            const int64_t current = _stream->Pos();
            _stream->ReadBuffer(ent, 16);
            // The last entry is followed by file data, not by a name length
            const int32_t nextLength = i < count ? _stream->I32() : 0;
            // Entry read correctly if the next name length and the offsets are plausible
            if (nextLength >= 0 && nextLength <= 255 && ent[3] > 12 && ent[3] <= total && ent[2] < total &&
                static_cast<int64_t>(ent[3]) + ent[2] <= total)
            {
                if (i < count)
                    _stream->Skip(-4);
            }
            else
            {
                // Junk between the name and the entry: the first byte is its length
                _stream->Seek(current);
                uint8_t junk = 0;
                _stream->ReadBuffer(&junk, 1);
                if (junk > 0)
                    _stream->Skip(junk - 1);
                _stream->ReadBuffer(ent, 16);
            }
            if (static_cast<int64_t>(ent[3]) + ent[2] > total)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, ent[3], ent[2]);
        }
    }
};
} // namespace

void RegisterFpk()
{
    Register<FpkArchive>("*.FPK", "Firaxis Archive (*.FPK)|Sid Meier's Civilization 4 (*.FPK)|Sid Meier's "
                                  "Civilization V (*.FPK)");
}
} // namespace Dud
