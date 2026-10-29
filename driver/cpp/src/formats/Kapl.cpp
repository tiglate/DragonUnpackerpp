// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Secret of Monkey Island Special Edition (and Monkey Island 2 SE) .PAK
// support ("KAPL"), ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (40 bytes): "KAPL", unknown, then the offsets of 4 blocks (unknown,
// index, names, data) and their 4 sizes. The index has 20 bytes per file (data
// offset relative to the data block, name offset, size, size again, unknown);
// the names block holds the null terminated names one after the other, in the
// order of the index.

#include "Registry.h"

namespace Dud
{
namespace
{
class KaplArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "KAPL") && stream.Size() >= 40;
    }

    void Parse() override
    {
        _stream->Seek(8);
        const int64_t unknownOffset = _stream->U32();
        const int64_t indexOffset = _stream->U32();
        const int64_t namesOffset = _stream->U32();
        const int64_t dataOffset = _stream->U32();
        const int64_t unknownSize = _stream->U32();
        const int64_t indexSize = _stream->U32();
        const int64_t namesSize = _stream->U32();
        const int64_t dataSize = _stream->U32();
        const int64_t total = _stream->Size();
        if (unknownOffset + unknownSize > total || indexOffset + indexSize > total ||
            namesOffset + namesSize > total || dataOffset + dataSize > total)
            Fail(DUDI7_ERR_NOTFORMAT, "A block is outside of the file");
        const int64_t count = indexSize / 20;
        std::vector<uint8_t> block(static_cast<size_t>(namesSize));
        _stream->Seek(namesOffset);
        if (namesSize > 0)
            _stream->ReadBuffer(block.data(), block.size());
        Reader names(std::move(block));

        _formatId = "KAPL";
        _dirSeparator = "/";
        for (int64_t i = 0; i < count; ++i)
        {
            _stream->Seek(indexOffset + i * 20);
            const uint32_t entDataOffset = _stream->U32();
            _stream->U32();
            const uint32_t size = _stream->U32();
            const uint32_t size2 = _stream->U32();
            const uint32_t unknown = _stream->U32();
            if (dataOffset + entDataOffset + size > total)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(names.CString(), dataOffset + entDataOffset, size, size2, unknown);
        }
    }
};
} // namespace

void RegisterKapl()
{
    Register<KaplArchive>("*.PAK", "The Secret of Monkey Island Special Edition (*.PAK)|Monkey Island 2 Special "
                                   "Edition (*.PAK)");
}
} // namespace Dud
