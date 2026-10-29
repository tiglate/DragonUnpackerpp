// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Zanzarah .PAK support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Starts with 4 zero bytes (so the extension is needed), then the number of
// entries and the entries: name (32 bits length + text), offset, size. Offsets
// are relative to the end of the directory + 4.

#include "Registry.h"

namespace Dud
{
namespace
{
class ZanzarahArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 8 || stream.U32() != 0)
            return false;
        const uint32_t count = stream.U32();
        if (count == 0 || count > 1000000)
            return false;
        std::vector<uint32_t> offsets(count);
        std::vector<uint32_t> sizes(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t length = stream.U32();
            // A real name: not empty, printable
            if (length == 0 || length > 260 || stream.Pos() + length + 8 > stream.Size())
                return false;
            std::string name(length, '\0');
            stream.ReadBuffer(name.data(), length);
            for (uint32_t j = 0; j < length; ++j)
            {
                if (static_cast<unsigned char>(name[j]) < ' ' && !(j == length - 1 && name[j] == '\0'))
                    return false;
            }
            offsets[i] = stream.U32();
            sizes[i] = stream.U32();
        }
        const int64_t base = stream.Pos() + 4;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (base + offsets[i] + sizes[i] > stream.Size())
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t count = _stream->U32();
        if (count == 0 || count > 1000000)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        std::vector<std::string> names(count);
        std::vector<uint32_t> offsets(count);
        std::vector<uint32_t> sizes(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            std::string name = _stream->LString32();
            // Files stored in the parent directory of the game
            if (name.compare(0, 3, "..\\") == 0)
                name.erase(0, 3);
            names[i] = std::move(name);
            offsets[i] = _stream->U32();
            sizes[i] = _stream->U32();
        }
        const int64_t base = _stream->Pos() + 4;

        _formatId = "ZPAK";
        _dirSeparator = "\\";
        for (uint32_t i = 0; i < count; ++i)
        {
            if (base + offsets[i] + sizes[i] > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(names[i], base + offsets[i], sizes[i]);
        }
    }
};
} // namespace

void RegisterZanzarah()
{
    Register<ZanzarahArchive>("*.PAK", "Zanzarah (*.PAK)");
}
} // namespace Dud
