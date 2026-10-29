// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Harbinger .SQH support, ported from drv_default.dpr of Dragon UnPACKer 5.
// No signature. The first 32 bits (little endian) is the end of the directory.
// Directory records (big endian numbers): name[8] then either, for a name without a
// dot, a folder: -1 (the folder ends where its parent ends) or the size in bytes of
// its content, followed by its records; or, for a file: offset, size.
// Only tried on .SQH files, the whole directory is validated.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int MaxDepth = 32;
constexpr int64_t MaxEntries = 1000000;

class SqhArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        const uint32_t limit = stream.U32();
        return limit > 4 && limit < stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint32_t limit = _stream->U32();
        if (limit <= 4 || limit >= _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory size");

        _formatId = "SQH";
        _dirSeparator = "\\";
        _extractInternal = false;
        ParseDir("", limit, 0);
        if (Count() == 0)
            Fail(DUDI7_ERR_NOTFORMAT, "No files");
    }

  private:
    void ParseDir(const std::string &dir, int64_t limit, int depth)
    {
        if (depth > MaxDepth)
            Fail(DUDI7_ERR_NOTFORMAT, "Folders nested too deep");
        while (_stream->Pos() < limit)
        {
            if (_stream->Pos() + 12 > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "Truncated directory");
            char buffer[8];
            _stream->ReadBuffer(buffer);
            const std::string name = ZString(buffer);
            if (name.empty())
                Fail(DUDI7_ERR_NOTFORMAT, "Empty name");
            for (const char c : name)
            {
                if (static_cast<unsigned char>(c) < ' ')
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid name");
            }
            const auto test = static_cast<int32_t>(_stream->BE32());
            if (name.find('.') == std::string::npos)
            {
                if (test == -1)
                {
                    ParseDir(dir + name + "\\", limit, depth + 1);
                }
                else
                {
                    if (test < 0 || _stream->Pos() + test > limit)
                        Fail(DUDI7_ERR_NOTFORMAT, "Invalid folder size");
                    ParseDir(dir + name + "\\", _stream->Pos() + test, depth + 1);
                }
            }
            else
            {
                const auto offset = static_cast<uint32_t>(test);
                const uint32_t size = _stream->BE32();
                if (static_cast<int64_t>(offset) + size > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                if (Count() >= MaxEntries)
                    Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
                Add(dir + name, offset, size);
            }
        }
    }
};
} // namespace

void RegisterSqh()
{
    Register<SqhArchive>("*.SQH", "Harbinger Archive (*.SQH)");
}
} // namespace Dud
