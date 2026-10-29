// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// AGON .SFL (SFL10) support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (29 bytes): "SFL10", offset of the folders, offset of the files, offset of the
// names, number of folders, number of files, size of the names. Files (17 bytes): name
// offset, 1 byte, id, offset, size. Names are null terminated strings in the names block.
// The folders are not used (v5 did not know how to link them to the files).

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t FileSize = 17;

class SflArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "SFL10");
    }

    void Parse() override
    {
        _stream->Seek(9);
        const uint32_t filesOffset = _stream->U32();
        const uint32_t namesOffset = _stream->U32();
        _stream->U32();
        const uint32_t fileCount = _stream->U32();
        const uint32_t namesLength = _stream->U32();
        if (static_cast<int64_t>(namesOffset) + namesLength > _stream->Size() ||
            fileCount > static_cast<uint64_t>(_stream->Size()) / FileSize)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");
        CheckTable(filesOffset, fileCount, FileSize);
        std::vector<char> names(static_cast<size_t>(namesLength) + 1, '\0');
        _stream->Seek(namesOffset);
        if (namesLength > 0)
            _stream->ReadBuffer(names.data(), namesLength);

        _formatId = "SFL10";
        _dirSeparator = "\\";
        _extractInternal = false;
        _stream->Seek(filesOffset);
        for (uint32_t i = 1; i <= fileCount; ++i)
        {
            const uint32_t nameOffset = _stream->U32();
            _stream->Skip(1);
            const uint32_t fileId = _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            if (nameOffset >= namesLength)
                Fail(DUDI7_ERR_NOTFORMAT, "A name is outside of the names block");
            uint32_t end = nameOffset;
            while (end < namesLength && names[end] != '\0')
                ++end;
            const std::string name(names.data() + nameOffset, end - nameOffset);
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, size, fileId);
        }
    }
};
} // namespace

void RegisterSfl()
{
    Register<SflArchive>("*.SFL", "AGON Archive (*.SFL)");
}
} // namespace Dud
