// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Act of War .DAT ("edat") support, ported from drv_default.dpr of Dragon UnPACKer 5 (based
// on the Xentax wiki page GRAF:Act_Of_War_DAT).
// Header (33 bytes): "edat", version $12, date, 2 null integers, 65536, 1 byte, offset of the
// directory, size of the directory (the directory ends the file). The directory is a list of
// records starting with an indicator: > 0 = a group (size of its content, name, even padding),
// 0 = a file (13 bytes: "not the last one of the group" indicator, offset, size, 1 byte,
// then the name and padding), < 0 = a file that shares the name found that many bytes
// back (13 bytes + 1 byte of padding).

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t Header = 33;
constexpr int MaxDepth = 32;

class EdatArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t h[8];
        if (stream.Read(h, 32) != 32)
            return false;
        return h[0] == 0x74616465 && h[1] == 0x12;
    }

    void Parse() override
    {
        _stream->Seek(25);
        const uint32_t dirOffset = _stream->U32();
        const uint32_t dirSize = _stream->U32();
        if (static_cast<int64_t>(dirOffset) + dirSize != _stream->Size() || dirOffset < Header)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory");
        _dir.resize(dirSize);
        _stream->Seek(dirOffset);
        if (dirSize > 0)
            _stream->ReadBuffer(_dir.data(), dirSize);
        _pos = 0;
        _formatId = "EDAT";
        _dirSeparator = "\\";
        _extractInternal = false;
        ParseGroup(dirSize, "", 0);
    }

  private:
    auto U32() -> int32_t
    {
        if (_pos < 0 || _pos + 4 > static_cast<int64_t>(_dir.size()))
            Fail(DUDI7_ERR_NOTFORMAT, "The directory is truncated");
        int32_t value;
        std::memcpy(&value, &_dir[static_cast<size_t>(_pos)], 4);
        _pos += 4;
        return value;
    }

    auto CStr() -> std::string
    {
        const int64_t size = static_cast<int64_t>(_dir.size());
        if (_pos < 0 || _pos >= size)
            Fail(DUDI7_ERR_NOTFORMAT, "The directory is truncated");
        int64_t e = _pos;
        while (e < size && _dir[static_cast<size_t>(e)] != 0)
            ++e;
        if (e >= size)
            Fail(DUDI7_ERR_NOTFORMAT, "Unterminated name");
        const std::string text(reinterpret_cast<const char *>(&_dir[static_cast<size_t>(_pos)]),
                               static_cast<size_t>(e - _pos));
        _pos = e + 1;
        return text;
    }

    void ParseGroup(int64_t endOffset, const std::string &group, int depth)
    {
        if (depth > MaxDepth)
            Fail(DUDI7_ERR_NOTFORMAT, "Groups nested too deep");
        if (_pos >= endOffset)
            return;
        do
        {
            const int64_t oldPos = _pos;
            const int32_t indicator = U32();
            if (indicator > 0)
            {
                const int32_t groupSize = U32();
                const std::string name = CStr();
                if ((name.size() + 1) % 2 == 1)
                    ++_pos;
                if (groupSize < 0 || _pos + groupSize > static_cast<int64_t>(_dir.size()))
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid group size");
                ParseGroup(_pos + groupSize, group + name + "\\", depth + 1);
            }
            else
            {
                const int32_t last = U32();
                const int32_t offset = U32();
                const int32_t size = U32();
                ++_pos;
                std::string name;
                if (indicator == 0)
                {
                    name = CStr();
                    if ((name.size() + 2) % 2 == 1)
                        ++_pos;
                }
                else
                {
                    ++_pos;
                    const int64_t cur = _pos;
                    _pos = oldPos + indicator;
                    name = CStr();
                    _pos = cur;
                }
                if (offset < 0 || size < 0 || static_cast<int64_t>(offset) + size > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                Add(group + name, offset, size);
                if (last == 0)
                    break;
            }
        } while (_pos < endOffset);
    }

    std::vector<uint8_t> _dir;
    int64_t _pos = 0;
};
} // namespace

void RegisterEdat()
{
    Register<EdatArchive>("*.DAT", "Act of War Archive (*.DAT)");
}
} // namespace Dud
