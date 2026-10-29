// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// LithTech .REZ support (AvP2, No One Lives Forever, Shogo...), ported from
// drv_default.dpr of Dragon UnPACKer 5.
// 127 bytes of text identifier, then 9 integers (version = 1, directory offset and
// size, ..., see below). The directory is a list of records (type, offset, size, date):
// type 1 = folder (name follows, offset/size = its own list of records), type 0 = file
// (numeric id, extension stored reversed on 4 bytes, number of keys, name follow).

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t HeaderSize = 163;
constexpr int MaxDepth = 32;
constexpr std::string_view IdNew = "\r\nRezMgr Version 1 Copyright (C) 1995 MONOLITH INC.           \r\n"
                                   "LithTech Resource File                                      \r\n\x1a";
constexpr std::string_view IdOld = "\r\nRezMgr Version 1 Copyright (C) 1995 MONOLITH INC.           \r\n"
                                   "                                                            \r\n\x1a";
static_assert(IdNew.size() == 127 && IdOld.size() == 127);

class RezArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char buffer[127];
        int32_t version = 0;
        if (stream.Read(buffer, 127) != 127 || stream.Read(&version, 4) != 4 || version != 1)
            return false;
        const std::string_view id(buffer, 127);
        return id == IdNew || id == IdOld;
    }

    void Parse() override
    {
        _stream->Seek(131);
        const uint32_t dirOffset = _stream->U32();
        const uint32_t dirSize = _stream->U32();
        if (dirOffset < HeaderSize || dirOffset >= _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory offset");

        _formatId = "REZ";
        _dirSeparator = "\\";
        _extractInternal = false;
        ParseDir(dirOffset, static_cast<int64_t>(dirOffset) + dirSize, "", 0);
    }

  private:
    void ParseDir(int64_t offset, int64_t maxOffset, const std::string &dir, int depth)
    {
        if (depth > MaxDepth || offset < 0 || offset >= _stream->Size())
            return;
        if (maxOffset > _stream->Size())
            maxOffset = _stream->Size();
        int64_t pos = offset;
        while (pos < maxOffset)
        {
            _stream->Seek(pos);
            const int32_t type = _stream->I32();
            const uint32_t entryOffset = _stream->U32();
            const uint32_t entrySize = _stream->U32();
            _stream->U32();
            switch (type)
            {
            case 1: {
                const std::string name = _stream->CString(1024);
                if (entrySize > 0)
                {
                    const int64_t resume = _stream->Pos();
                    ParseDir(entryOffset, static_cast<int64_t>(entryOffset) + entrySize, dir + name + "\\", depth + 1);
                    _stream->Seek(resume);
                }
                break;
            }
            case 0: {
                _stream->U32();
                char ext[4];
                _stream->ReadBuffer(ext);
                _stream->U32();
                std::string name = _stream->CString(1024);
                std::string extension;
                for (int i = 0; i < 4 && ext[i] != '\0'; ++i)
                    extension.insert(extension.begin(), ext[i]);
                if (!name.empty() && entryOffset > 162 && entrySize > 0 && ext[3] == '\0' &&
                    static_cast<int64_t>(entryOffset) + entrySize <= _stream->Size())
                {
                    if (!extension.empty())
                        name += "." + extension;
                    Add(dir + name, entryOffset, entrySize);
                }
                break;
            }
            default:
                Fail(DUDI7_ERR_NOTFORMAT, "Unknown directory record type");
            }
            pos = _stream->Pos();
        }
    }
};
} // namespace

void RegisterRez()
{
    Register<RezArchive>("*.REZ", "LithTech Resource (*.REZ)|Alien vs Predator 2 (*.REZ)|No One Lives Forever "
                                  "(*.REZ)|Shogo (*.REZ)");
}
} // namespace Dud
