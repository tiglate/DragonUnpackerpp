// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Warlords Battlecry (1 and 2) .XCR support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (28 bytes): "xcr File 1.00" (13 bytes), 7 empty bytes, number of entries, size of the file. Entry (532
// bytes): file name (256), full name with the folders (256), offset, size, 12 unknown bytes. The names that
// start with a drive ("C:\") lose it.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr std::string_view Id = "xcr File 1.00";
constexpr int64_t HeaderSize = 28;
constexpr int64_t EntrySize = 532;

class XcrArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (!StartsWith(stream, Id))
            return false;
        stream.Seek(24);
        uint32_t size;
        return stream.Read(&size, 4) == 4 && size == stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(20);
        const int32_t numE = _stream->I32();
        const int32_t fileSize = _stream->I32();
        if (fileSize != _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid file size");
        if (numE < 0 || static_cast<int64_t>(numE) * EntrySize + HeaderSize > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        _formatId = "XCR";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (int32_t x = 1; x <= numE; ++x)
        {
            char fileName[256], dirName[256];
            _stream->ReadBuffer(fileName);
            _stream->ReadBuffer(dirName);
            const int32_t offset = _stream->I32();
            const int32_t size = _stream->I32();
            _stream->Skip(12);
            std::string name = ZString(dirName);
            if (name.size() >= 3 && name[1] == ':' && name[2] == '\\')
                name.erase(0, 3);
            if (offset < 0 || size < 0 || static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, size);
        }
    }
};
} // namespace

void RegisterXcr()
{
    Register<XcrArchive>("*.XCR", "Warlords Battlecry Archive (*.XCR)");
}
} // namespace Dud
