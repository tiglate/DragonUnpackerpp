// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Ensemble Studios .BAR support (ported from drv_default.dpr of Dragon
// UnPACKer 5): Age of Empires 3 ("ESPN" signature) and Age of Mythology.
//
// Age of Empires 3: 292 bytes header (the number of entries is at 280 and the
// directory offset at 284). The directory starts with the base directory name
// and the entries count again, then entries of 28 bytes (offset, size, ..)
// each followed by its name. Texts are a 32 bits length (in characters)
// followed by UTF-16 characters.
//
// Age of Mythology: 28 bytes header starting with 8 zero bytes (so the
// extension is needed to recognize it): number of entries at 12, directory
// offset at 20. Entries (20 bytes: offset, size, ..) are at the directory
// offset + 4 * number of entries, each followed by a null terminated name.

#include "Registry.h"

namespace Dud
{
namespace
{
class Aoe3BarArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "ESPN") && stream.Size() >= 292;
    }

    void Parse() override
    {
        _stream->Seek(280);
        const uint32_t count = _stream->U32();
        const uint32_t dirOffset = _stream->U32();
        CheckTable(dirOffset, 1, 8);

        _formatId = "BAR";
        _dirSeparator = "\\";
        _stream->Seek(dirOffset);
        const std::string baseDir = _stream->WString32();
        _stream->U32();
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            _stream->Skip(20);
            Add(baseDir + _stream->WString32(), offset, size);
        }
    }
};

class AomBarArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint8_t id[8];
        if (stream.Read(id, 8) != 8 || stream.Size() < 28)
            return false;
        for (const uint8_t b : id)
        {
            if (b != 0)
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(12);
        const uint32_t count = _stream->U32();
        _stream->U32();
        const uint32_t dirOffset = _stream->U32();
        const int64_t tableStart = static_cast<int64_t>(dirOffset) + 4 * static_cast<int64_t>(count);
        CheckTable(tableStart, count, 21);

        _formatId = "BAR";
        _dirSeparator = "\\";
        _stream->Seek(tableStart);
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            _stream->U32();
            const uint32_t unknown1 = _stream->U32();
            const uint32_t unknown2 = _stream->U32();
            Add(_stream->CString(), offset, size, unknown1, unknown2);
        }
    }
};
} // namespace

void RegisterBar()
{
    Register<Aoe3BarArchive>("*.BAR", "Age of Mythology (*.BAR)|Age of Empires 3 (*.BAR)");
    Register<AomBarArchive>("*.BAR", "Age of Mythology (*.BAR)");
}
} // namespace Dud
