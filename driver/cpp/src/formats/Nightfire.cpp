// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// James Bond 007: NightFire .007 support (demo = version 1, retail = version 3), ported from
// drv_default.dpr of Dragon UnPACKer 5.
// Header (20 bytes): 1, version, unknown, "ROOT", number of folders in the root (version 3 then
// has the total number of entries). The tree is a sequence: a file is name (32 bits length +
// chars) then compressed flag (8 bits), size, packed size; a folder is 0, its name, its number
// of sub folders (version 3: and its number of entries) followed by its content, which ends
// at a 0 (not consumed). Version 1 has the data right after each file record, version 3 has
// it after the tree (and 4 bytes), in the order of the files. Packed files are zlib streams.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int MaxDepth = 64;
constexpr size_t MaxEntries = 1000000;

class NightFireArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t h[5];
        if (stream.Read(h, 20) != 20)
            return false;
        return h[0] == 1 && h[3] == 0x544F4F52 && (h[1] == 1 || h[1] == 3);
    }

    void Parse() override
    {
        _stream->Seek(4);
        _version = _stream->I32();
        _stream->Seek(16);
        const int32_t numRoot = _stream->I32();
        if (_version == 3)
            _stream->I32();
        if (numRoot < 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of folders");
        _formatId = "007";
        _dirSeparator = "\\";
        _extractInternal = true;
        ParseDir("", numRoot, 0);
        if (_version == 3)
        {
            // the data follows the tree, in the order of the files
            int64_t data = _stream->Pos() + 4;
            for (Entry &entry : _entries)
            {
                if (data + entry.DataY > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                entry.Offset = data;
                data += entry.DataY;
            }
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == 0)
            return Archive::Extract(index, sink);
        return ExtractZlib(entry.Offset, entry.DataY, entry.Size, sink);
    }

  private:
    // a file record (the 32 bits length of its name has been peeked, not consumed)
    void ReadFile(const std::string &dir)
    {
        const std::string name = _stream->LString32(255);
        uint8_t comp;
        _stream->ReadBuffer(&comp, 1);
        const uint32_t size = _stream->U32();
        uint32_t compSize = _stream->U32();
        if (_entries.size() >= MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
        if (comp == 0)
            compSize = size;
        if (_version == 1)
        {
            // the data follows the record
            if (_stream->Pos() + compSize > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(dir + name, _stream->Pos(), size, comp, compSize);
            _stream->Seek(_stream->Pos() + compSize);
        }
        else
            Add(dir + name, 0, size, comp, compSize);
    }

    void ParseDir(const std::string &dir, int32_t numDirs, int depth)
    {
        if (depth > MaxDepth)
            Fail(DUDI7_ERR_NOTFORMAT, "Folders nested too deep");
        if (numDirs == 0)
        {
            // no folder: the files go on until a 0 (which is not consumed)
            uint32_t check = 1;
            while (_stream->Pos() < _stream->Size() && check != 0)
            {
                check = _stream->U32();
                _stream->Seek(_stream->Pos() - 4);
                if (check != 0)
                    ReadFile(dir);
            }
        }
        else
            while (_stream->Pos() < _stream->Size() && numDirs > 0)
            {
                const uint32_t check = _stream->U32();
                if (check == 0)
                {
                    const std::string name = _stream->LString32(255);
                    const int32_t sub = _stream->I32();
                    if (_version == 3)
                        _stream->I32();
                    if (sub < 0)
                        Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of folders");
                    ParseDir(dir + name + "\\", sub, depth + 1);
                    --numDirs;
                }
                else
                {
                    _stream->Seek(_stream->Pos() - 4);
                    ReadFile(dir);
                }
            }
    }

    int32_t _version = 0;
};
} // namespace

void RegisterNightfire()
{
    Register<NightFireArchive>("*.007", "James Bond 007: NightFire (*.007)");
}
} // namespace Dud
