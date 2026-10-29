// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tony Hawk's Pro Skater 2 .PKR support, ported from drv_default.dpr of Dragon UnPACKer 5.
// "PKR2", version, number of folders, number of files. Folders (40 bytes): name[32],
// unknown, number of files. Files (48 bytes): name[32], unknown, offset, size, size.
// Files are listed folder after folder.

#include "Registry.h"

namespace Dud
{
namespace
{
class PkrArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "PKR2");
    }

    void Parse() override
    {
        _stream->Seek(8);
        const int32_t folderCount = _stream->I32();
        const int32_t fileCount = _stream->I32();
        if (folderCount < 0 || fileCount < 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");
        CheckTable(16, folderCount, 40);
        CheckTable(16 + static_cast<int64_t>(folderCount) * 40, fileCount, 48);
        std::vector<std::string> dirNames(static_cast<size_t>(folderCount));
        std::vector<int32_t> dirCounts(static_cast<size_t>(folderCount));
        for (size_t i = 0; i < dirNames.size(); ++i)
        {
            char name[32];
            _stream->ReadBuffer(name);
            dirNames[i] = ZString(name);
            _stream->U32();
            dirCounts[i] = _stream->I32();
        }

        _formatId = "PKR";
        _dirSeparator = "/";
        _extractInternal = false;
        size_t current = 0;
        for (int32_t i = 1; i <= fileCount; ++i)
        {
            char name[32];
            _stream->ReadBuffer(name);
            _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            _stream->U32();
            while (current < dirNames.size() && dirCounts[current] <= 0)
                ++current;
            if (current >= dirNames.size())
                Fail(DUDI7_ERR_NOTFORMAT, "More files than the folders announce");
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(dirNames[current] + ZString(name), offset, size);
            --dirCounts[current];
        }
    }
};
} // namespace

void RegisterPkr()
{
    Register<PkrArchive>("*.PKR", "Tony Hawk's Pro Skater 2 Archive (*.PKR)");
}
} // namespace Dud
