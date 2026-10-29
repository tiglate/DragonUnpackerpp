// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The 7th Guest .GJD support, ported from drv_default.dpr of Dragon UnPACKer 5.
// The .GJD file has no directory: it is in the file with the same name and the extension .RL, a table of 20 bytes
// entries (name 12 bytes, offset, size) that can describe several .GJD files, so only the entries that fit in
// this one are kept. Only files with the extension .GJD that have their .RL next to them are tried.
// The 11th Hour also uses .GJD files, but v5 needs a data file built from the CD with a separate tool
// (drv_default_d11d.dat); it is not supported.

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t EntrySize = 20;

auto OpenRl(Reader &gjd) -> std::unique_ptr<Reader>
{
    const std::string rl = Sibling(gjd.Path(), ".rl");
    if (rl.empty())
        return nullptr;
    try
    {
        return std::make_unique<Reader>(rl);
    }
    catch (const Error &)
    {
        return nullptr;
    }
}

class Gjd7Archive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        const std::unique_ptr<Reader> rl = OpenRl(stream);
        if (!rl || rl->Size() < EntrySize || rl->Size() % EntrySize != 0)
            return false;
        // at least one entry fits in this file, and the names look like names
        int fitting = 0;
        for (int64_t i = 0; i < rl->Size() / EntrySize; ++i)
        {
            char name[12];
            rl->Seek(i * EntrySize);
            if (rl->Read(name, 12) != 12)
                return false;
            if (name[0] == '\0')
                return false;
            for (char c : name)
                if (c != '\0' && (static_cast<unsigned char>(c) < ' ' || static_cast<unsigned char>(c) > '~'))
                    return false;
            uint32_t offset, size;
            if (rl->Read(&offset, 4) != 4 || rl->Read(&size, 4) != 4)
                return false;
            if (static_cast<int64_t>(offset) + size <= stream.Size())
                ++fitting;
        }
        return fitting > 0;
    }

    void Parse() override
    {
        const std::unique_ptr<Reader> rl = OpenRl(*_stream);
        if (!rl)
            Fail(DUDI7_ERR_NOTFORMAT, "No .RL file next to the .GJD");
        _formatId = "GJD";
        _dirSeparator = "";
        _extractInternal = false;
        const int64_t total = rl->Size() / EntrySize;
        rl->Seek(0);
        for (int64_t x = 0; x < total; ++x)
        {
            char name[12];
            rl->ReadBuffer(name);
            const uint32_t offset = rl->U32();
            const uint32_t size = rl->U32();
            if (static_cast<int64_t>(offset) + size <= _stream->Size())
                Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterGjd7()
{
    Register<Gjd7Archive>("*.GJD", "The 7th Guest Archive (*.GJD)");
}
} // namespace Dud
