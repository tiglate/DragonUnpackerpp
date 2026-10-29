// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Descent / Descent 2 .HOG support (ported from drv_default.dpr of Dragon
// UnPACKer 5). "DHF" followed by the files one after the other, each one
// preceded by its name (13 bytes) and its size (32 bits).
// Descent 3 "HOG2" files: "HOG2", number of files, offset of the data (32 bits each), 56 unused bytes, then the
// entries (name 36 bytes, unknown integer, size, time stamp: 48 bytes); the files follow one after the other from
// the offset of the data. (v5 added 48 bytes to every offset; the real files, Descent 3 on GOG, show that the data
// of the first file starts at the offset itself and that the sizes add up to the end of the file.)

#include "Registry.h"

namespace Dud
{
namespace
{
class HogArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "DHF") || StartsWith(stream, "HOG2");
    }

    void ParseHog2()
    {
        _formatId = "HOG2";
        _dirSeparator = "";
        _stream->Seek(4);
        const int32_t numFiles = _stream->I32();
        const int32_t dataOffset = _stream->I32();
        const int64_t total = _stream->Size();
        if (numFiles < 0 || dataOffset < 0 || dataOffset > total || 68 + static_cast<int64_t>(numFiles) * 48 > total)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");
        _stream->Seek(68);
        int64_t pos = dataOffset;
        for (int32_t x = 0; x < numFiles; ++x)
        {
            char name[36];
            _stream->ReadBuffer(name);
            _stream->U32();
            const int32_t size = _stream->I32();
            _stream->U32();
            if (size < 0 || pos + size > total)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(name), pos, size);
            pos += size;
        }
    }

    void Parse() override
    {
        if (StartsWith(*_stream, "HOG2"))
        {
            ParseHog2();
            return;
        }
        _formatId = "HOG";
        _dirSeparator = "";
        const int64_t total = _stream->Size();
        int64_t pos = 3;
        while (pos < total)
        {
            if (pos + 17 > total)
                Fail(DUDI7_ERR_NOTFORMAT, "Truncated HOG entry header");
            _stream->Seek(pos);
            char name[13];
            _stream->ReadBuffer(name);
            const int32_t size = _stream->I32();
            pos += 17;
            if (size < 0 || pos + size > total)
                Fail(DUDI7_ERR_NOTFORMAT, "HOG entry is outside of the file");
            Add(ZString(name), pos, size);
            pos += size;
        }
    }
};
} // namespace

void RegisterHog()
{
    Register<HogArchive>("*.HOG;*.MN3", "Descent (*.HOG)|Descent 2 (*.HOG)|Descent 3 (*.HOG;*.MN3)");
}
} // namespace Dud
