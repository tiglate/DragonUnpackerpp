// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// LucasArts .LFD resource files (Dark Forces, Dark Forces Remaster, X-Wing, TIE Fighter).
// The file starts with a master entry "RMAP"/"resource" whose size is the length of the directory that
// follows (16 bytes per resource: type[4], name[8], size[4]); then every resource comes in the order of
// the directory, preceded by a copy of its 16 bytes header. Entries are named "name.type".

#include <cstring>

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
auto SizeOf(const char *entry) -> int64_t
{
    uint32_t value;
    std::memcpy(&value, entry + 12, 4);
    return value;
}

class LfdArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char h[16];
        if (stream.Size() < 16 || stream.Read(h, 16) != 16)
            return false;
        if (std::string_view(h, 4) != "RMAP")
            return false;
        const int64_t dir = SizeOf(h);
        if (dir % 16 != 0 || 16 + dir > stream.Size())
            return false;
        // the resources have to fill the file exactly
        int64_t pos = 16 + dir;
        for (int64_t i = 0; i < dir / 16; ++i)
        {
            stream.Seek(16 + i * 16);
            char e[16];
            if (stream.Read(e, 16) != 16)
                return false;
            const int64_t size = SizeOf(e);
            if (pos + 16 + size > stream.Size())
                return false;
            pos += 16 + size;
        }
        return pos == stream.Size();
    }

    void Parse() override
    {
        _formatId = "DLFD";
        _dirSeparator = "";
        _stream->Seek(12);
        const int64_t dir = _stream->U32();
        int64_t pos = 16 + dir;
        for (int64_t i = 0; i < dir / 16; ++i)
        {
            _stream->Seek(16 + i * 16);
            char e[16];
            _stream->ReadBuffer(e, 16);
            const int64_t size = SizeOf(e);
            std::string type(e, 4);
            std::string name(e + 4, 8);
            const size_t nul = name.find('\0');
            if (nul != std::string::npos)
                name.resize(nul);
            // Trim: characters up to the space on both ends
            size_t first = 0, last = type.size();
            while (first < last && static_cast<unsigned char>(type[first]) <= ' ')
                ++first;
            while (last > first && static_cast<unsigned char>(type[last - 1]) <= ' ')
                --last;
            Add(name + "." + ToLower(type.substr(first, last - first)), pos + 16, size);
            pos += 16 + size;
        }
    }
};
} // namespace

void RegisterLfd()
{
    Register<LfdArchive>("*.LFD", "LucasArts resource (*.LFD)");
}
} // namespace Dud
