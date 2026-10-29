// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Leisure Suit Larry: Magna Cum Laude .JAM ("JAM2") support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (32 bytes): "JAM2", creation date, offset of the first file, 16 unknown bytes, number of file names,
// number of extensions (16 bits each). Then the file names (8 bytes, padded with zeros), the extensions (4 bytes),
// a 16 bits count of root entries (followed by 2 unknown bytes) and the entries of the directory index (8 bytes:
// name index, extension index, offset). An entry whose offset is before the first file is a folder: the high
// 4 bits of the name index plus the extension index divided by 32 give the number of entries (1 when 0) found at
// that offset, the other bits the name of the folder. Any other entry is a file: at its offset a 32 bytes header
// (size, size again, 6 unknown integers) precedes the data. v5 would stop with an exception on an index outside
// the lists; here the entry is refused.

#include <cstdio>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int MaxDepth = 64;
constexpr size_t MaxEntries = 1000000;

class JamArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "JAM2") && stream.Size() >= 32;
    }

    void Parse() override
    {
        _stream->Seek(4);
        _stream->U32(); // creation date
        _firstData = _stream->U32();
        _stream->Skip(16);
        const uint16_t numFiles = _stream->U16();
        const uint16_t numExts = _stream->U16();
        if (_firstData < 32 || _firstData > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid offset of the first file");
        for (uint16_t x = 0; x < numFiles; ++x)
        {
            char name[8];
            _stream->ReadBuffer(name);
            _names.push_back(ZString(name));
        }
        for (uint16_t x = 0; x < numExts; ++x)
        {
            char ext[4];
            _stream->ReadBuffer(ext);
            _exts.push_back(ZString(ext));
        }
        const uint16_t rootCount = _stream->U16();
        _stream->Skip(2);
        const int64_t rootAt = _stream->Pos();
        _formatId = "JAM2";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (uint16_t x = 0; x < rootCount; ++x)
            Read("", rootAt + static_cast<int64_t>(x) * 8, 0);
    }

  private:
    // Reads the entry of the directory index at offset; returns the number of files found
    auto Read(const std::string &curDir, int64_t offset, int depth) -> int
    {
        if (depth > MaxDepth)
            Fail(DUDI7_ERR_NOTFORMAT, "Folders nested too deep");
        if (offset >= _firstData || offset + 8 > _stream->Size())
            return 0; // (something is wrong: there is no directory data after the first file)
        _stream->Seek(offset);
        uint16_t fileIdx = _stream->U16();
        const uint16_t extIdx = _stream->U16();
        const uint32_t dirOffset = _stream->U32();
        int result = 0;
        if (dirOffset < _firstData)
        {
            // a folder
            int numDirs = ((fileIdx & 0xF000) >> 12) + extIdx / 0x20;
            if (numDirs == 0)
                numDirs = 1;
            fileIdx &= 0xFFF;
            // (in the bounds: the name of the folder, otherwise a name made of the index)
            char unknown[16];
            std::snprintf(unknown, sizeof unknown, "UNK-%04X", fileIdx);
            const std::string part = fileIdx < _names.size() ? _names[fileIdx] : std::string(unknown);
            const std::string newDir = curDir.empty() ? part : curDir + "\\" + part;
            for (int x = 1; x <= numDirs; ++x)
            {
                const int64_t at = static_cast<int64_t>(dirOffset) + (x - 1) * 8;
                if (at >= _firstData)
                    break;
                const int found = Read(newDir, at, depth + 1);
                if (found == 0)
                    break;
                result += found;
            }
        }
        else
        {
            // a file
            if (fileIdx >= _names.size() || extIdx >= _exts.size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file name or extension does not exist");
            if (static_cast<int64_t>(dirOffset) + 0x20 > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            _stream->Seek(dirOffset);
            const uint32_t size = _stream->U32();
            if (static_cast<int64_t>(dirOffset) + 0x20 + size > _stream->Size() || _entries.size() >= MaxEntries)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            const std::string name = _names[fileIdx] + "." + _exts[extIdx];
            Add(curDir.empty() ? name : curDir + "\\" + name, static_cast<int64_t>(dirOffset) + 0x20, size);
            ++result;
        }
        return result;
    }

    uint32_t _firstData = 0;
    std::vector<std::string> _names, _exts;
};
} // namespace

void RegisterJam()
{
    Register<JamArchive>("*.JAM", "Leisure Suit Larry: Magna Cum Laude Archive (*.JAM)");
}
} // namespace Dud
