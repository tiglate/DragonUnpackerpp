// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// EVE Online .STUFF support, ported from drv_default.dpr of Dragon UnPACKer 5 (based on the
// infos of DaReverse). No signature: number of entries, then for each: size, 4 unknown
// bytes, null terminated name; the files follow, one after the other. Only tried on
// .STUFF files, the sizes must fit in the file.

#include "Registry.h"

namespace Dud
{
namespace
{
class StuffArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        const uint32_t count = stream.U32();
        if (!(count >= 1 && count <= static_cast<uint64_t>(stream.Size()) / 10))
            return false;
        int64_t sum = 0;
        for (uint32_t i = 1; i <= count; ++i)
        {
            const uint32_t size = stream.U32();
            stream.U32();
            const std::string name = stream.CString(1024);
            if (name.empty())
                return false;
            for (const char c : name)
            {
                if (static_cast<unsigned char>(c) < ' ')
                    return false;
            }
            sum += size;
        }
        return stream.Pos() + sum <= stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint32_t count = _stream->U32();
        if (count < 1 || count > static_cast<uint64_t>(_stream->Size()) / 10)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        std::vector<uint32_t> sizes(count);
        std::vector<std::string> names(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            sizes[i] = _stream->U32();
            _stream->U32();
            names[i] = _stream->CString(1024);
            if (names[i].empty())
                Fail(DUDI7_ERR_NOTFORMAT, "Empty name");
        }
        int64_t data = _stream->Pos();

        _formatId = "STUFF";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (data + sizes[i] > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(names[i], data, sizes[i]);
            data += sizes[i];
        }
    }
};
} // namespace

void RegisterStuff()
{
    Register<StuffArchive>("*.STUFF", "EVE Online Archive (*.STUFF)");
}
} // namespace Dud
