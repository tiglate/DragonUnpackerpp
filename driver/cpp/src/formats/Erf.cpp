// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Dragon Age: Origins .ERF support (version 2.0)
// (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (32 bytes): "ERF V2.0" in UTF-16, number of entries, 3 unknown
// values. Entries are 72 bytes: name (32 UTF-16 characters), offset, size.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
class ErfArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        constexpr char expected[16] = {'E', 0, 'R', 0, 'F', 0, ' ', 0, 'V', 0, '2', 0, '.', 0, '0', 0};
        char id[16];
        return stream.Read(id, 16) == 16 && std::memcmp(id, expected, 16) == 0;
    }

    void Parse() override
    {
        _stream->Seek(16);
        const uint32_t count = _stream->U32();
        CheckTable(32, count, 72);

        _formatId = "ERF";
        _dirSeparator = "/";
        _stream->Seek(32);
        for (uint32_t i = 0; i < count; ++i)
        {
            char16_t name[32];
            _stream->ReadBuffer(name, sizeof name);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            size_t n = 0;
            while (n < 32 && name[n] != u'\0')
                ++n;
            Add(Utf16ToUtf8(std::u16string(name, n)), offset, size);
        }
    }
};
} // namespace

void RegisterErf()
{
    Register<ErfArchive>("*.ERF", "Dragon Age: Origins (*.ERF)");
}
} // namespace Dud
