// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Lionhead audio banks (Black & White .SAD, Black & White 2 and Fable: The Lost Chapters .LUG), ported from
// drv_default.dpr of Dragon UnPACKer 5. "LiOnHeAd", then chunks (name 32 bytes, size). The chunk
// "LHAudioWaveData" holds the sounds; "LHAudioBankSampleTable" starts with two 16 bits counts followed by
// the entries (size = chunk size minus 4, divided by the count; 640 bytes in Black & White, 652 in the others):
// sample name (path of the original machine, 256 bytes), 12 bytes, size, offset in the wave data... The banks of
// music hold the sectors of an MPEG video as hundreds of samples named "sect*.mpg": they become one file.

#include <cstring>
#include <set>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MinEntrySize = 276;
constexpr uint32_t MaxEntrySize = 652;

auto Lower(std::string s) -> std::string
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return s;
}

class LionheadAudioArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (!StartsWith(stream, "LiOnHeAd"))
            return false;
        Chunks chunks;
        return Scan(stream, chunks) && chunks.EntrySize != 0;
    }

    void Parse() override
    {
        Chunks chunks;
        if (!Scan(*_stream, chunks) || chunks.EntrySize == 0)
            Fail(DUDI7_ERR_NOTFORMAT, "No sound table or no sound data");
        _formatId = "LHAB";
        _dirSeparator = "\\";
        _extractInternal = false;

        _stream->Seek(chunks.DirOffset);
        std::vector<uint8_t> raw(chunks.EntrySize);
        std::set<int32_t> seen;
        bool isMusic = true, isFirst = true;
        int64_t musicOffset = 0, musicSize = 0;
        for (uint32_t x = 0; x < chunks.DirNum; ++x)
        {
            _stream->ReadBuffer(raw.data(), raw.size());
            std::string disp = ZString(reinterpret_cast<const char *>(raw.data()), 256);
            int32_t size, relOffset;
            std::memcpy(&size, raw.data() + 268, 4);
            std::memcpy(&relOffset, raw.data() + 272, 4);
            if (disp.size() >= 3 && disp[1] == ':' && disp[2] == '\\')
                disp = disp.substr(3);
            const size_t slash = disp.find_last_of("\\/");
            const std::string nam = slash == std::string::npos ? disp : disp.substr(slash + 1);
            if (size <= 0 || !seen.insert(relOffset).second)
                continue;
            if (relOffset < 0 || chunks.DataOffset + relOffset + static_cast<int64_t>(size) > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A sample is outside of the archive");
            const std::string lower = Lower(nam);
            isMusic = isMusic && lower.size() >= 4 && lower.compare(0, 4, "sect") == 0 && lower.size() >= 4 &&
                      lower.compare(lower.size() - 4, 4, ".mpg") == 0;
            if (isMusic)
            {
                if (isFirst)
                {
                    musicOffset = relOffset;
                    isFirst = false;
                }
                musicSize += size;
            }
            else
                Add(disp, chunks.DataOffset + relOffset, size);
        }
        if (isMusic)
        {
            _entries.clear();
            if (musicOffset + chunks.DataOffset + musicSize > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "The music is outside of the archive");
            std::string nam = _stream->Path();
            const size_t slash = nam.find_last_of("\\/");
            if (slash != std::string::npos)
                nam = nam.substr(slash + 1);
            const size_t dot = nam.find_last_of('.');
            if (dot != std::string::npos)
                nam.resize(dot);
            Add(nam + ".mpg", chunks.DataOffset + musicOffset, musicSize);
        }
    }

  private:
    struct Chunks
    {
        int64_t DataOffset = 0;
        int64_t DirOffset = 0;
        uint32_t DirNum = 0;
        uint32_t EntrySize = 0;
    };

    static auto Scan(Reader &stream, Chunks &chunks) -> bool
    {
        const int64_t total = stream.Size();
        int64_t dirSize = 0;
        int64_t pos = 8;
        std::string tag;
        try
        {
            do
            {
                if (pos + 36 > total)
                    break;
                stream.Seek(pos);
                char id[32];
                stream.ReadBuffer(id);
                const int32_t size = stream.I32();
                const int64_t cur = pos + 36;
                tag = ZString(id);
                if (tag == "LHAudioWaveData")
                    chunks.DataOffset = cur;
                else if (tag == "LHAudioBankSampleTable")
                {
                    chunks.DirNum = stream.U16();
                    chunks.DirOffset = cur + 4;
                    dirSize = static_cast<int64_t>(size) - 4;
                }
                if (size < 0)
                    return false;
                pos = cur + size;
            } while (!tag.empty() && pos < total);
        }
        catch (const ReadError &)
        {
            return false;
        }
        if (chunks.DirOffset <= 0 || chunks.DataOffset <= 0 || dirSize <= 0 || chunks.DirNum == 0)
            return false;
        const uint32_t entrySize = static_cast<uint32_t>(dirSize / chunks.DirNum);
        if (entrySize < MinEntrySize || entrySize > MaxEntrySize || dirSize % chunks.DirNum != 0 ||
            chunks.DirOffset + dirSize > total)
            return false;
        chunks.EntrySize = entrySize;
        return true;
    }
};
} // namespace

void RegisterLionheadAudio()
{
    Register<LionheadAudioArchive>("*.LUG;*.SAD", "Black & White, Black & White 2, Fable (*.LUG;*.SAD)");
}
} // namespace Dud
