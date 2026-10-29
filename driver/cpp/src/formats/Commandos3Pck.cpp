// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Commandos 3: Destination Berlin .PCK support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Entries of 48 bytes: name[36], flag (0 file, 1 folder, 0xFF end of the folder), size, offset. The first one is
// the root folder (flag 1, size and offset 0xFFFFFFFF/0x30); the entries of a folder follow the entry of the
// folder itself, up to the one with the flag 0xFF. Most files are encrypted: the first byte of every 16 bytes is
// XORed with a key that depends on the position, found from the first byte and the kind of the file (DataX holds
// the value the first byte has to give, chosen by extension as in v5, DataY the mode).

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int MaxDepth = 64;
constexpr size_t MaxEntries = 1000000;
constexpr int64_t Chunk = 65536;

auto Upper(std::string s) -> std::string
{
    for (char &c : s)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return s;
}

struct Rule
{
    const char *Name;
    int Seed;
    int Mode;
};

// Extension (or whole name) -> value of the first byte, mode (1: FNC/FNM, 2: GSC/BAS)
constexpr Rule ByExtension[] = {
    {".WAV", 0x52, 0}, {".STR", 0xFF, 0}, {".FAC", 0x5B, 0}, {".CFG", 0x5B, 0}, {".ABI", 0x4C, 0}, {".GRL", 0x47, 0},
    {".MAN", 0x28, 0}, {".MAC", 0x54, 0}, {".TUT", 0x5B, 0}, {".MBI", 0x32, 0}, {".SEC", 0x02, 0}, {".FNC", 0, 1},
    {".FNM", 0, 1},    {".ANI", 0x42, 0}, {".AN2", 0x42, 0}, {".MSB", 0x42, 0}, {".BRI", 0x42, 0}, {".OGC", 0x20, 0},
    {".MIS", 0x5B, 0}, {".LIS", 0x5B, 0}, {".GMT", 0x5B, 0}, {".MA2", 0x03, 0}, {".GSC", 0x5B, 2}, {".BAS", 0x5B, 2},
};
constexpr Rule ByName[] = {
    {"FILE.BIN", 0x42, 0}, {"CHUSMA.DAT", 0x42, 0}, {"BER2.MIS", 0x42, 0}, {"VAR.DAT", 0x23, 0},
    {"MISIONES.DAT", 0x2F, 0}, {"PRGE.DAT", 0x28, 0}, {"PARGLOBAL.DAT", 0x5B, 0},
};

class Commandos3PckArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 96)
            return false;
        char name[36];
        stream.ReadBuffer(name);
        const int32_t flag = stream.I32();
        const uint32_t size = stream.U32();
        const uint32_t offset = stream.U32();
        return flag == 1 && size == 0xFFFFFFFFu && offset == 0x30;
    }

    void Parse() override
    {
        _stream->Seek(0);
        char name[36];
        _stream->ReadBuffer(name);
        _stream->Skip(12);
        std::string root = ZString(name);
        if (!root.empty())
            root += "\\";
        _formatId = "PCK";
        _dirSeparator = "\\";
        _extractInternal = true;
        ReadFolder(root, 0);
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        int64_t seed = entry.DataX;
        const int64_t size = entry.Size;
        if (entry.Offset < 0 || entry.Offset + size > _stream->Size())
        {
            LastError = "Entry is outside of the file";
            return DUDI7_ERR_IO;
        }
        bool encrypted = true;
        int key = 0;
        bool fixedKey = false;
        if (entry.DataY == 1)
        {
            if (size < 17)
            {
                LastError = "Entry is too short";
                return DUDI7_ERR_IO;
            }
            _stream->Seek(entry.Offset + 16);
            int t = static_cast<uint8_t>((_stream->U8() ^ 0x2C) - 0x10);
            t = t == 0 ? 0xE : t == 8 ? 6 : t - 1;
            key = t;
            fixedKey = true;
        }
        else if (entry.DataY == 2 || entry.DataX != 0)
        {
            if (size >= 4)
            {
                char id[3];
                _stream->Seek(entry.Offset + 1);
                _stream->ReadBuffer(id);
                if (std::memcmp(id, "IFF", 3) == 0)
                    seed = 0x52;
                else if (std::memcmp(id, "DMB", 3) == 0)
                    seed = 0x4C;
                else if (std::memcmp(id, "FRL", 3) == 0)
                    seed = 0x47;
                else if (std::memcmp(id, "SMB", 3) == 0)
                    seed = 0x42;
            }
        }
        else
            encrypted = false;
        if (!encrypted)
            return Archive::Extract(index, sink);

        _stream->Seek(entry.Offset);
        std::vector<uint8_t> buffer(Chunk);
        int64_t done = 0;
        bool found = false;
        int skeleton = 0;
        while (done < size)
        {
            const int64_t length = std::min<int64_t>(Chunk, size - done);
            _stream->ReadBuffer(buffer.data(), static_cast<size_t>(length));
            if (!found)
            {
                // the key of the first block makes the first byte give the seed (FNC/FNM: the key is known)
                if (!fixedKey)
                    key = (buffer[0] ^ static_cast<int>(seed)) & 0xFF;
                found = true;
            }
            for (int64_t x = 0; x * 16 < length; ++x)
            {
                buffer[static_cast<size_t>(x * 16)] ^= static_cast<uint8_t>(key + skeleton);
                skeleton = skeleton == 240 ? 0 : skeleton + 16;
                ++key;
                if (key == 7)
                    key = 8;
                if (key >= 15)
                    key = 0;
            }
            if (!sink.Write(buffer.data(), length))
            {
                LastError = "Extraction stopped by the host";
                return DUDI7_ERR_CANCELLED;
            }
            done += length;
        }
        return DUDI7_OK;
    }

  private:
    auto ReadFolder(const std::string &curDir, int depth) -> int
    {
        if (depth > MaxDepth)
            Fail(DUDI7_ERR_NOTFORMAT, "Folders nested too deep");
        int result = 0;
        int32_t flag;
        do
        {
            char raw[36];
            _stream->ReadBuffer(raw);
            flag = _stream->I32();
            const uint32_t size = _stream->U32();
            const uint32_t offset = _stream->U32();
            const std::string name = ZString(raw);
            if (flag == 0)
            {
                if (static_cast<int64_t>(offset) + size > _stream->Size() || _entries.size() >= MaxEntries)
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                int seed = 0, mode = 0;
                const std::string upper = Upper(name);
                const size_t dot = upper.rfind('.');
                const std::string ext = dot == std::string::npos ? "" : upper.substr(dot);
                bool matched = false;
                for (const Rule &r : ByName)
                    if (upper == r.Name)
                        seed = r.Seed, mode = r.Mode, matched = true;
                if (!matched)
                    for (const Rule &r : ByExtension)
                        if (ext == r.Name)
                            seed = r.Seed, mode = r.Mode;
                Add(curDir + name, offset, size, seed, mode);
                ++result;
            }
            else if (flag == 1)
                result += ReadFolder(curDir + name + "\\", depth + 1);
        } while (flag != 0xFF);
        return result;
    }
};
} // namespace

void RegisterCommandos3Pck()
{
    Register<Commandos3PckArchive>("*.PCK", "Commandos 3 (*.PCK)");
}
} // namespace Dud
