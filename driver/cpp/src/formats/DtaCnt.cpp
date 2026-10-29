// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Hidden & Dangerous .DTA/.CNT support, ported from drv_default.dpr of Dragon UnPACKer 5.
// The .DTA file ("DTA_") holds the data, the .CNT file of the same name the directory: "COMPCNT4", the size of
// the .DTA file, 2 unknown integers, the number of entries, then for each: offset, size (32 bits) and the name
// (16 bits length + characters). Entries of size 0 are left out.

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
// The companion directory, opened and checked (null when the file is not a .CNT of this .DTA)
auto OpenCnt(Reader &dta) -> std::unique_ptr<Reader>
{
    const std::string cnt = Sibling(dta.Path(), ".cnt");
    if (cnt.empty())
        return nullptr;
    try
    {
        auto reader = std::make_unique<Reader>(cnt);
        char id[8];
        reader->Seek(0);
        if (reader->Read(id, 8) != 8 || std::string_view(id, 8) != "COMPCNT4")
            return nullptr;
        if (reader->I32() != dta.Size())
            return nullptr;
        return reader;
    }
    catch (const Error &)
    {
        return nullptr;
    }
}

class DtaCntArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "DTA_") && OpenCnt(stream) != nullptr;
    }

    void Parse() override
    {
        std::unique_ptr<Reader> cnt = OpenCnt(*_stream);
        if (!cnt)
            Fail(DUDI7_ERR_NOTFORMAT, "No valid .CNT file next to the .DTA");
        cnt->Seek(20);
        int32_t numE = cnt->I32();
        if (numE < 0 || numE > (cnt->Size() - 24) / 10)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        _formatId = "DTA/CNT";
        _dirSeparator = "\\";
        _extractInternal = true;
        for (int32_t x = 1; x <= numE; ++x)
        {
            const int32_t offset = cnt->I32();
            const int32_t size = cnt->I32();
            const uint16_t length = cnt->U16();
            std::string name(length, '\0');
            if (length > 0)
                cnt->ReadBuffer(name.data(), length);
            name = ZString(name.data(), length);
            if (size > 0)
            {
                if (offset < 0 || static_cast<int64_t>(offset) + size > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                Add(name, offset, size);
            }
        }
    }
};
} // namespace

void RegisterDtaCnt()
{
    Register<DtaCntArchive>("*.DTA", "Hidden & Dangerous Archive (*.DTA)");
}
} // namespace Dud
