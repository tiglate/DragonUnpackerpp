// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Emperor: Battle for Dune / F1 Manager 2000 .RFH/.RFD (and .RFC) support, ported from
// drv_default.dpr of Dragon UnPACKer 5. The data is in the .RFD file, the index in the
// .RFH file (or in the .RFC file when there is one): open the .RFH, the others are looked
// for in the same directory.
// .RFH records: 24 bytes (name length, date, compression, packed size, size, offset)
// then the null terminated name. Compression 1 = packed data listed and copied as is,
// 2 = 4 bytes then zlib data (packed size includes the 4 bytes).
// .RFC: a count then records of 28 bytes (unknown, compression, packed size, size,
// offset, unknown, name length) followed by the name; packed files are listed as is,
// with the extension ".cmpN", as in Dragon UnPACKer 5.

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
class RfhArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        return ToUpper(ExtensionOf(stream.Path())) == ".RFH" && !Sibling(stream.Path(), ".rfd").empty() &&
               stream.Size() >= 24;
    }

    void Parse() override
    {
        const std::string rfhName = _stream->Path();
        const std::string rfdName = Sibling(rfhName, ".rfd");
        const std::string rfcName = Sibling(rfhName, ".rfc");
        _stream = std::make_unique<Reader>(rfdName);
        const int64_t endSize = _stream->Size();
        _formatId = "RFH/RFD";
        _dirSeparator = "\\";
        _extractInternal = true;
        Reader index(rfcName.empty() ? rfhName : rfcName);
        if (!rfcName.empty())
        {
            const uint32_t count = index.U32();
            if (static_cast<int64_t>(count) * 28 > index.Size())
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
            for (uint32_t i = 1; i <= count; ++i)
            {
                uint32_t ent[7];
                index.ReadBuffer(ent, 28);
                const uint32_t flag = ent[1];
                const uint32_t packed = ent[2];
                const uint32_t original = ent[3];
                const uint32_t offset = ent[4];
                if (offset > endSize || static_cast<int64_t>(offset) + packed > endSize || ent[6] > 255)
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                std::string name(ent[6], '\0');
                if (!name.empty())
                    index.ReadBuffer(name.data(), name.size());
                name = ZString(name.data(), name.size());
                if (flag != 0)
                    Add(name + ".cmp" + std::to_string(flag), offset, packed, 0, 0);
                else
                    Add(name, offset, original, 0, 0);
            }
        }
        else
        {
            while (index.Pos() < index.Size())
            {
                if (index.Pos() + 24 > index.Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "Truncated index");
                uint32_t ent[6];
                index.ReadBuffer(ent, 24);
                const uint32_t flag = ent[2];
                const uint32_t packed = ent[3];
                const uint32_t original = ent[4];
                const uint32_t offset = ent[5];
                if (offset > endSize || static_cast<int64_t>(offset) + packed > endSize)
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                const std::string name = index.CString(1024);
                if (flag == 2)
                    Add(name, offset, original, 2, packed);
                else if (flag == 1)
                    Add(name, offset, packed, 1, packed);
                else
                    Add(name, offset, original, 0, packed);
            }
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == 2 && entry.DataY >= 4)
            return ExtractZlib(entry.Offset + 4, entry.DataY - 4, entry.Size, sink);
        return Archive::Extract(index, sink);
    }
};
} // namespace

void RegisterRfh()
{
    Register<RfhArchive>("*.RFH", "Emperor: Battle for Dune (*.RFH)|F1 Manager 2000 (*.RFH)");
}
} // namespace Dud
