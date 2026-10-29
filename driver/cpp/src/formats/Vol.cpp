// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Dynamix / Starsiege: Tribes .VOL support ("PVOL"), ported from
// drv_default.dpr of Dragon UnPACKer 5 (see the Xentax wiki page "Star Siege").
// Header: "PVOL", offset of the "vols" chunk. Chunks are a 4 characters tag
// and a size (32 bits): "vols" holds the null terminated file names, "voli"
// the index (17 bytes per file: unknown, id, offset, size, unknown byte).
// File offsets point to a chunk header (tag, size) of 8 bytes before the data.

#include "Registry.h"

namespace Dud
{
namespace
{
class VolArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "PVOL");
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t offset = _stream->U32();
        if (static_cast<int64_t>(offset) + 8 > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "The names chunk is outside of the file");
        _stream->Seek(offset);
        char id[4];
        _stream->ReadBuffer(id);
        uint32_t size = _stream->U32();
        if (!Equals(id, "vols") || static_cast<int64_t>(offset) + 8 + size > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "The names chunk (vols) was not found");
        std::vector<char> names(size);
        if (size > 0)
            _stream->ReadBuffer(names.data(), size);
        size_t namePos = 0;

        // The next chunk is word aligned
        const int64_t pos = static_cast<int64_t>(offset) + 8 + size + (size % 2);
        if (pos + 8 > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "The index chunk is outside of the file");
        _stream->Seek(pos);
        _stream->ReadBuffer(id);
        size = _stream->U32();
        if (!Equals(id, "voli") || pos + 8 + size > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "The index chunk (voli) was not found");

        _formatId = "PVOL";
        _dirSeparator = "";
        const uint32_t count = size / 17;
        for (uint32_t i = 0; i < count; ++i)
        {
            _stream->U32();
            const uint32_t fileId = _stream->U32();
            const uint32_t fileOffset = _stream->U32();
            const uint32_t fileSize = _stream->U32();
            _stream->U8();
            if (static_cast<int64_t>(fileOffset) + 8 + fileSize > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(NextName(names, namePos), static_cast<int64_t>(fileOffset) + 8, fileSize, fileId);
        }
    }

  private:
    // Next null terminated text of the names chunk (same limits as Reader::CString)
    static auto NextName(const std::vector<char> &names, size_t &pos) -> std::string
    {
        constexpr size_t maxLength = 1024;
        std::string result;
        do
        {
            if (pos >= names.size())
                Fail(DUDI7_ERR_IO, "Unexpected end of file");
            const char c = names[pos++];
            if (c == '\0')
                return result;
            result += c;
        } while (result.size() <= maxLength);
        Fail(DUDI7_ERR_NOTFORMAT, "Text too long");
    }
};
} // namespace

void RegisterVol()
{
    Register<VolArchive>("*.VOL", "Starsiege: Tribes (*.VOL)");
}
} // namespace Dud
