// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Battleforge .PAK support ("PAK" + 1), ported from drv_default.dpr of Dragon
// UnPACKer 5. Header (16 bytes): "PAK", version (1), directory offset, directory
// size, compressed directory size. The directory is zlib compressed; once
// unpacked: number of entries then for each a name (32 bits length + text),
// an offset and an END offset (the size is end - offset).

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxDirSize = 256 * 1024 * 1024;

class BfPakArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, std::string_view("PAK\x01", 4));
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t dirOffset = _stream->U32();
        const uint32_t dirSize = _stream->U32();
        const uint32_t dirCompressed = _stream->U32();
        if (static_cast<int64_t>(dirOffset) + dirCompressed > _stream->Size() || dirSize > MaxDirSize || dirSize < 4)
            Fail(DUDI7_ERR_NOTFORMAT, "The directory is outside of the file");
        std::vector<uint8_t> data;
        try
        {
            data = Unpack(*_stream, dirOffset, dirSize);
        }
        catch (const Error &e)
        {
            Fail(DUDI7_ERR_NOTFORMAT, std::string("Cannot unpack the directory: ") + e.what());
        }
        Reader dir(std::move(data));
        const uint32_t count = dir.U32();

        _formatId = "BFPAK";
        _dirSeparator = "\\";
        for (uint32_t i = 0; i < count; ++i)
        {
            const std::string name = dir.LString32();
            const uint32_t offset = dir.U32();
            const uint32_t endOffset = dir.U32();
            if (endOffset < offset || static_cast<int64_t>(endOffset) > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, endOffset - offset);
        }
    }
};
} // namespace

void RegisterBfpak()
{
    Register<BfPakArchive>("*.PAK", "Battleforge (*.PAK)");
}
} // namespace Dud
