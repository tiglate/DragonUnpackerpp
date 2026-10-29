// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Ubisoft BigFile ("UBI_BF_SIG") .M4B support (Myst IV: Revelation), ported from
// drv_default.dpr of Dragon UnPACKer 5.
// 11 (32 bits), "UBI_BF_SIG" + null, 1, 0, then a recursive directory: a byte N; if N = 0
// it is a list of files: number of files, then name (32 bits length + chars), size, offset;
// else N sub directories follow: name length (32 bits), name, and its content; the list ends
// with an unknown 32 bits integer.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int MaxDepth = 32;
constexpr uint32_t MaxEntries = 1000000;

class UbiBfArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t sig;
        char h[11];
        if (stream.Read(&sig, 4) != 4 || sig != 11 || stream.Read(h, 11) != 11)
            return false;
        return std::string_view(h, 10) == "UBI_BF_SIG" && h[10] == '\0';
    }

    void Parse() override
    {
        _stream->Seek(15 + 8);
        _formatId = "M4B";
        _dirSeparator = "\\";
        _extractInternal = false;
        ParseDir("", 0);
    }

  private:
    void ParseDir(const std::string &dir, int depth)
    {
        if (depth > MaxDepth)
            Fail(DUDI7_ERR_NOTFORMAT, "Folders nested too deep");
        uint8_t n;
        _stream->ReadBuffer(&n, 1);
        if (n == 0)
        {
            const uint32_t num = _stream->U32();
            if (num > MaxEntries)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of files");
            for (uint32_t j = 1; j <= num; ++j)
            {
                const std::string name = _stream->LString32(1024);
                const uint32_t size = _stream->U32();
                const uint32_t offset = _stream->U32();
                if (static_cast<int64_t>(offset) + size > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                if (!dir.empty())
                    Add(dir + "\\" + name, offset, size);
                else
                    Add("\\" + name, offset, size);
            }
        }
        else
        {
            for (int i = 1; i <= n; ++i)
            {
                const uint32_t len = _stream->U32();
                if (len > 65535)
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid name length");
                std::string name(len, '\0');
                if (len > 0)
                    _stream->ReadBuffer(name.data(), len);
                name = ZString(name.data(), len);
                if (!dir.empty())
                    name = dir + "\\" + name;
                ParseDir(name, depth + 1);
            }
            _stream->U32();
        }
    }
};
} // namespace

void RegisterUbibf()
{
    Register<UbiBfArchive>("*.M4B;*.BF", "Ubisoft BigFile (*.M4B;*.BF)|Myst IV: Revelation (*.M4B)");
}
} // namespace Dud
