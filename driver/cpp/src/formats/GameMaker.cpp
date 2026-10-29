// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// GameMaker data files (data.win, game.unx, game.droid, game.ios: Undertale, Hyper Light Drifter...).
// Not in Dragon UnPACKer 5. An IFF "FORM" of chunks (tag, size), the first being GEN8. Offsets inside the
// chunks are absolute positions in the file. The entries are the embedded audio of AUDO (count, offsets;
// each blob: size + data, WAV or Ogg; named after the sound of SOND that uses it, strings are pointers
// to characters preceded by their length) as audio/<name>.wav|ogg, and the texture pages of TXTR
// (PNG files, their length is found by walking the PNG chunks) as textures/texture_<n>.png.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
class GameMakerArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char h[12];
        if (stream.Size() < 20 || stream.Read(h, 12) != 12)
            return false;
        uint32_t size;
        std::memcpy(&size, h + 4, 4);
        return std::memcmp(h, "FORM", 4) == 0 && std::memcmp(h + 8, "GEN8", 4) == 0 &&
               static_cast<int64_t>(size) + 8 <= stream.Size();
    }

    void Parse() override
    {
        _formatId = "GMFORM";
        _dirSeparator = "/";
        int64_t last = static_cast<int64_t>(FileU32(4)) + 8;
        if (last > _stream->Size())
            last = _stream->Size();
        int64_t sondP = 0, sondN = 0, audoP = 0, audoN = 0, txtrP = 0, txtrN = 0;
        int64_t p = 8;
        while (p + 8 <= last)
        {
            _stream->Seek(p);
            char tag[4];
            _stream->ReadBuffer(tag, 4);
            const int64_t size = _stream->U32();
            const std::string_view tagText(tag, 4);
            if (p + 8 + size > last)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid chunk");
            if (tagText == "SOND")
            {
                sondP = p + 8;
                sondN = size;
            }
            else if (tagText == "AUDO")
            {
                audoP = p + 8;
                audoN = size;
            }
            else if (tagText == "TXTR")
            {
                txtrP = p + 8;
                txtrN = size;
            }
            p += 8 + size;
        }
        // the names of the sounds by audio index (SOND: name, flags, extension, file, effects, volume, pitch,
        // group, audio id)
        if (audoP > 0 && audoN >= 4)
        {
            const int64_t count = FileU32(audoP);
            if (count * 4 + 4 > audoN)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid AUDO");
            std::vector<std::string> names(static_cast<size_t>(count));
            if (sondP > 0)
            {
                const int64_t q = FileU32(sondP);
                if (q * 4 + 4 <= sondN)
                    for (int64_t i = 0; i < q; ++i)
                    {
                        const int64_t ptr = FileU32(sondP + 4 + i * 4);
                        if (ptr + 36 > _stream->Size())
                            continue;
                        const int64_t flags = FileU32(ptr + 4);
                        const int64_t id = FileU32(ptr + 32);
                        const int64_t nameP = FileU32(ptr);
                        if ((flags & 1) != 0 && id < count && names[static_cast<size_t>(id)].empty() && nameP > 4 &&
                            nameP < _stream->Size())
                        {
                            const int64_t length = FileU32(nameP - 4);
                            if (length > 0 && length < 200 && nameP + length <= _stream->Size())
                            {
                                std::string name(static_cast<size_t>(length), '\0');
                                _stream->Seek(nameP);
                                _stream->ReadBuffer(name.data(), name.size());
                                names[static_cast<size_t>(id)] = name;
                            }
                        }
                    }
            }
            for (int64_t i = 0; i < count; ++i)
            {
                const int64_t off = FileU32(audoP + 4 + i * 4);
                if (off < audoP || off + 4 > _stream->Size())
                    continue;
                const int64_t size = FileU32(off);
                if (off + 4 + size > _stream->Size())
                    continue;
                const char *ext = ".bin";
                if (size >= 4)
                {
                    _stream->Seek(off + 4);
                    char sig[4];
                    _stream->ReadBuffer(sig, 4);
                    if (std::memcmp(sig, "RIFF", 4) == 0)
                        ext = ".wav";
                    else if (std::memcmp(sig, "OggS", 4) == 0)
                        ext = ".ogg";
                }
                const std::string &known = names[static_cast<size_t>(i)];
                const std::string name =
                    known.empty() ? "audio_" + std::to_string(i) : known + "_" + std::to_string(i);
                Add("audio/" + name + ext, off + 4, size);
            }
        }
        // texture pages: entries are (scaled, pointer to the PNG), the PNG ends with its IEND chunk
        if (txtrP > 0 && txtrN >= 4)
        {
            const int64_t count = FileU32(txtrP);
            if (count * 4 + 4 <= txtrN)
                for (int64_t i = 0; i < count; ++i)
                {
                    const int64_t ptr = FileU32(txtrP + 4 + i * 4);
                    if (ptr == 0 || ptr + 8 > _stream->Size())
                        continue;
                    const int64_t q = FileU32(ptr + 4);
                    if (q == 0 || q + 8 > _stream->Size())
                        continue;
                    _stream->Seek(q);
                    uint8_t b[8];
                    _stream->ReadBuffer(b, 8);
                    if (!(b[0] == 0x89 && b[1] == 0x50 && b[2] == 0x4E && b[3] == 0x47))
                        continue;
                    // walk the chunks of the PNG
                    int64_t pngEnd = q + 8;
                    while (pngEnd + 12 <= _stream->Size())
                    {
                        _stream->Seek(pngEnd);
                        _stream->ReadBuffer(b, 8);
                        const uint32_t chunkLength = (static_cast<uint32_t>(b[0]) << 24) |
                                                     (static_cast<uint32_t>(b[1]) << 16) |
                                                     (static_cast<uint32_t>(b[2]) << 8) | b[3];
                        pngEnd += 12 + static_cast<int64_t>(chunkLength);
                        if (b[4] == 'I' && b[5] == 'E' && b[6] == 'N' && b[7] == 'D')
                            break;
                    }
                    if (pngEnd > _stream->Size())
                        continue;
                    Add("textures/texture_" + std::to_string(i) + ".png", q, pngEnd - q);
                }
        }
        if (Count() == 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Nothing to extract");
    }

  private:
    auto FileU32(int64_t position) -> uint32_t
    {
        if (position < 0 || position + 4 > _stream->Size())
            return 0;
        _stream->Seek(position);
        return _stream->U32();
    }
};
} // namespace

void RegisterGameMaker()
{
    Register<GameMakerArchive>("*.WIN;*.UNX;*.DROID;*.IOS", "GameMaker data (data.win, game.unx)");
}
} // namespace Dud
