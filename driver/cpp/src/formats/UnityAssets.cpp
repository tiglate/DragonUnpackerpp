// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Stand-alone Unity serialized files: the data files of a player (globalgamemanagers, levelN,
// sharedassetsN.assets, resources.assets...). Not in Dragon UnPACKer 5. Only the textures, sounds,
// texts and fonts are listed ("objects/<Class>/<name>"; see UnitySerial and UnityExport),
// the data they keep in the companion files (.resS, .resource) is read from the files of the
// same folder. The files have no marker: the header must be consistent (size of the file,
// version 17 to 22) and the whole list of types and objects must be readable.

#include <filesystem>

#include "PathUtil.h"
#include "Registry.h"
#include "UnityObjects.h"
#include "codecs/UnitySerial.h"

namespace Dud
{
namespace
{
constexpr int SrcShift = 40; // positions: source number (0 = the file) and offset

auto SameText(const std::string &a, const std::string &b) -> bool
{
    return ToLower(a) == ToLower(b);
}

class UnityAssetsArchive : public UnityObjectArchive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (stream.Size() < 48)
            return false;
        stream.Seek(0);
        uint8_t hdr[24];
        if (stream.Read(hdr, 24) != 24)
            return false;
        const uint32_t ver = (static_cast<uint32_t>(hdr[8]) << 24) | (static_cast<uint32_t>(hdr[9]) << 16) |
                             (static_cast<uint32_t>(hdr[10]) << 8) | hdr[11];
        if (ver < 17 || ver > 22)
            return false;
        int64_t size;
        if (ver < 22)
            size = (static_cast<int64_t>(hdr[4]) << 24) | (static_cast<int64_t>(hdr[5]) << 16) |
                   (static_cast<int64_t>(hdr[6]) << 8) | hdr[7];
        else
        {
            stream.Seek(24);
            if (stream.Read(hdr, 8) != 8)
                return false;
            size = static_cast<int64_t>((static_cast<uint64_t>(hdr[0]) << 56) | (static_cast<uint64_t>(hdr[1]) << 48) |
                                        (static_cast<uint64_t>(hdr[2]) << 40) | (static_cast<uint64_t>(hdr[3]) << 32) |
                                        (static_cast<uint64_t>(hdr[4]) << 24) | (static_cast<uint64_t>(hdr[5]) << 16) |
                                        (static_cast<uint64_t>(hdr[6]) << 8) | hdr[7]);
        }
        if (size != stream.Size())
            return false;
        UnitySerialized serialized(
            [&stream](int64_t pos, int64_t len, void *dst) -> bool {
                if (pos < 0 || len < 0 || pos + len > stream.Size())
                    return false;
                if (len == 0)
                    return true;
                stream.Seek(pos);
                return stream.Read(dst, static_cast<size_t>(len)) == static_cast<size_t>(len);
            },
            0, stream.Size());
        return serialized.Load();
    }

    void Parse() override
    {
        _formatId = "UnityAssets";
        _dirSeparator = "/";
        AddObjects(0, _stream->Size(), "objects/");
        // (the positions of the data streams carry the number of their file, not wanted in the listing)
        for (Entry &entry : _entries)
            entry.Offset &= (static_cast<int64_t>(1) << SrcShift) - 1;
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        return ExtractVirtual(_virt[static_cast<size_t>(_entries[static_cast<size_t>(index)].DataX) - 1], sink);
    }

  protected:
    auto ReadAt(int64_t pos, int64_t len, void *dst) -> bool override
    {
        const int n = static_cast<int>(pos >> SrcShift);
        pos &= (static_cast<int64_t>(1) << SrcShift) - 1;
        Reader *src;
        if (n == 0)
            src = _stream.get();
        else if (n <= static_cast<int>(_sources.size()))
            src = _sources[static_cast<size_t>(n) - 1].get();
        else
            return false;
        if (len < 0 || pos + len > src->Size())
            return false;
        if (len == 0)
            return true;
        src->Seek(pos);
        return src->Read(dst, static_cast<size_t>(len)) == static_cast<size_t>(len);
    }

    // The data streams are files of the folder of the archive (same name, case may differ)
    auto ResolveStream(const std::string &path, int64_t off, int64_t size, int64_t &pos) -> bool override
    {
        const size_t slash = path.find_last_of('/');
        const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
        if (base.empty() || off < 0 || size < 0)
            return false;
        const std::string &own = _stream->Path();
        const size_t ownSlash = own.find_last_of('/');
        if (SameText(base, ownSlash == std::string::npos ? own : own.substr(ownSlash + 1)))
        {
            if (off + size > _stream->Size())
                return false;
            pos = off;
            return true;
        }
        for (size_t i = 0; i < _sourceNames.size(); ++i)
            if (_sourceNames[i] == base)
            {
                if (off + size > _sources[i]->Size())
                    return false;
                pos = (static_cast<int64_t>(i + 1) << SrcShift) + off;
                return true;
            }
        const std::string dir = ownSlash == std::string::npos ? "" : own.substr(0, ownSlash + 1);
        std::string found;
        if (FileExists(dir + base))
            found = dir + base;
        else
        {
            std::error_code error;
            const std::filesystem::path folder = dir.empty() ? std::filesystem::path(".")
                                                              : std::filesystem::path(std::u8string(dir.begin(), dir.end()));
            for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error))
            {
                const std::u8string name = it->path().filename().u8string();
                const std::string text(name.begin(), name.end());
                if (SameText(text, base))
                {
                    found = dir + text;
                    break;
                }
            }
        }
        if (found.empty())
            return false;
        std::unique_ptr<Reader> file;
        try
        {
            file = std::make_unique<Reader>(found);
        }
        catch (const Error &)
        {
            return false;
        }
        _sources.push_back(std::move(file));
        _sourceNames.push_back(base);
        if (off + size > _sources.back()->Size())
            return false;
        pos = (static_cast<int64_t>(_sources.size()) << SrcShift) + off;
        return true;
    }

  private:
    std::vector<std::unique_ptr<Reader>> _sources;
    std::vector<std::string> _sourceNames;
};
} // namespace

void RegisterUnityAssets()
{
    Register<UnityAssetsArchive>("*.ASSETS", "Unity serialized file (*.ASSETS, levelN, globalgamemanagers)");
}
} // namespace Dud
