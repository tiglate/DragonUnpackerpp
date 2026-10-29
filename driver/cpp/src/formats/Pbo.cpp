// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Operation Flashpoint .PBO support (ported from drv_default.dpr of Dragon
// UnPACKer 5). No signature: a list of entries, each a null terminated name and
// 20 bytes (packing method, original size, reserved, time stamp, data size),
// ended by an entry with an empty name; the data of all the files follows, in
// the same order. Newer PBO files (ArmA) start with an empty name ("Vers"
// header) and are not supported. The extension is needed.

#include "Registry.h"

namespace Dud
{
namespace
{
class PboArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        // The first name cannot be empty
        stream.Seek(0);
        char c = 0;
        return stream.Read(&c, 1) == 1 && c != '\0';
    }

    void Parse() override
    {
        _stream->Seek(0);
        std::vector<std::string> names;
        std::vector<uint32_t> sizes;
        bool slash = true;
        for (;;)
        {
            std::string name = _stream->CString();
            // 12 bytes: method, original size, reserved; time stamp; data size
            _stream->Skip(12 + 4);
            const uint32_t size = _stream->U32();
            if (name.empty())
                break;
            if (name.find('\\') != std::string::npos)
                slash = false;
            names.push_back(std::move(name));
            sizes.push_back(size);
            if (names.size() > 1000000)
                Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
        }
        if (names.empty())
            Fail(DUDI7_ERR_NOTFORMAT, "No file in the PBO");

        _formatId = "PBO";
        _dirSeparator = slash ? "/" : "\\";
        int64_t offset = _stream->Pos();
        for (size_t i = 0; i < names.size(); ++i)
        {
            if (offset + sizes[i] > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(names[i], offset, sizes[i]);
            offset += sizes[i];
        }
    }
};
} // namespace

void RegisterPbo()
{
    Register<PboArchive>("*.PBO", "Operation Flashpoint (*.PBO)");
}
} // namespace Dud
