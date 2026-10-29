// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// FMOD sound banks. Not in Dragon UnPACKer 5.
// FSB5 (.fsb, also what Unity keeps in the .resource of its sound clips): "FSB5", version (0 or 1),
// number of samples, size of the sample headers, size of the names table, size of the data, codec
// (mode), then 32 bytes (version 0: 28) of hash and padding. Each sample header is 64 bits: bit 0
// extra data follows, bits 1-4 frequency index, bits 5-6 channels - 1, bits 7-33 position of the
// data in 32 bytes units, bits 34-63 number of samples; the extra chunks (32 bits: bit 0 more
// chunks, bits 1-24 size, bits 25-31 type) can give the real frequency (type 2). Names table:
// 32 bits offsets (from the start of the table) then the texts. The size of a sample is the
// distance to the next one. The entries are the sounds: PCM ones get a WAV header, MPEG ones are
// valid .mp3 files, other codecs (Vorbis needs a setup header that is not stored, ADPCM...) are
// given as they are in "<name>.<codec>.bin".
// FMOD Studio banks (.bank): RIFF "FEV " holding chunks FMT, LIST (the events: not read) and SND
// (an FSB5, after some padding). Banks without sounds (Master, strings) have no entries.
// FSB4 is not supported.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>

#include "PathUtil.h"
#include "Registry.h"
#include "codecs/FsbCodebooks.h"
#include "codecs/VorbisOgg.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxMeta = 64 * 1024 * 1024;
constexpr uint32_t MaxSamples = 1000000;
constexpr int64_t WavHeader = 44;
constexpr const char *CodecNames[16] = {"none", "pcm8", "pcm16",   "pcm24", "pcm32", "float", "gcadpcm", "imaadpcm",
                                        "vag",  "hevag", "xma",    "mpeg",  "celt",  "at9",   "xwma",    "vorbis"};
constexpr int Freqs[10] = {4000, 8000, 11000, 11025, 16000, 22050, 24000, 32000, 44100, 48000};

// how an entry is made
constexpr uint8_t KindRaw = 0;
constexpr uint8_t KindWav = 1;  // PCM with a WAV header
constexpr uint8_t KindMpeg = 2; // frames of Layer III/II without the padding of FMOD
constexpr uint8_t KindOgg = 3;  // Vorbis packets in an Ogg file

auto Le32(const uint8_t *b, int64_t p) -> uint32_t
{
    return static_cast<uint32_t>(b[p]) | (static_cast<uint32_t>(b[p + 1]) << 8) |
           (static_cast<uint32_t>(b[p + 2]) << 16) | (static_cast<uint32_t>(b[p + 3]) << 24);
}

auto IsFsb5(Reader &s, int64_t pos) -> bool
{
    if (pos < 0 || pos + 60 > s.Size())
        return false;
    s.Seek(pos);
    uint8_t h[28];
    if (s.Read(h, 28) != 28)
        return false;
    if (h[0] != 'F' || h[1] != 'S' || h[2] != 'B' || h[3] != '5')
        return false;
    const uint32_t ver = Le32(h, 4), ns = Le32(h, 8), shs = Le32(h, 12), nts = Le32(h, 16), mode = Le32(h, 24);
    return ver <= 1 && mode <= 15 && ns <= MaxSamples && shs <= MaxMeta && nts <= MaxMeta &&
           static_cast<int64_t>(ns) * 8 <= shs;
}

// Length of the MPEG audio frame whose header is h (0: not a header)
auto MpegFrameLen(uint32_t h, int &layer) -> int
{
    static const uint16_t br[5][16] = {
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0}, // MPEG 1 layer I
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},    // MPEG 1 layer II
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},     // MPEG 1 layer III
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},    // MPEG 2 layer I
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}};        // MPEG 2 layers II and III
    static const uint16_t sr1[3] = {44100, 48000, 32000};
    layer = 0;
    if ((h >> 21) != 0x7FF)
        return 0;
    const int ver = static_cast<int>((h >> 19) & 3); // 3 = MPEG 1, 2 = MPEG 2, 0 = MPEG 2.5
    const int lay = 4 - static_cast<int>((h >> 17) & 3); // 1..3 (0 is reserved)
    const int bi = static_cast<int>((h >> 12) & 15);
    const int si = static_cast<int>((h >> 10) & 3);
    const int pad = static_cast<int>((h >> 9) & 1);
    if (ver == 1 || lay > 3 || ((h >> 17) & 3) == 0 || bi == 0 || bi == 15 || si == 3)
        return 0;
    layer = lay;
    int row, rate;
    if (ver == 3)
    {
        row = lay - 1;
        rate = sr1[si];
    }
    else
    {
        row = lay == 1 ? 3 : 4;
        rate = sr1[si] / 2;
        if (ver == 0)
            rate /= 2;
    }
    const int bitrate = br[row][bi] * 1000;
    switch (lay)
    {
    case 1:
        return (12 * bitrate / rate + pad) * 4;
    case 2:
        return 144 * bitrate / rate + pad;
    default:
        return ver == 3 ? 144 * bitrate / rate + pad : 72 * bitrate / rate + pad;
    }
}

class FsbBase : public Archive
{
  public:
    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const auto at = static_cast<size_t>(index);
        const Entry &entry = _entries[at];
        const int64_t freq = entry.DataY >> 8;
        const int64_t chans = entry.DataY & 255;
        switch (_kind[at])
        {
        case KindMpeg: {
            int64_t total;
            return MpegFrames(entry.Offset, _srcLen[at], _pad[at], &sink, total);
        }
        case KindOgg: {
            const std::string setup = FsbSetupPacket(_crc[at]);
            VorbisModes modes;
            if (setup.empty() || !ParseVorbisSetup(setup, static_cast<int>(chans), modes))
            {
                LastError = "Unknown Vorbis setup";
                return DUDI7_ERR_UNSUPPORTED;
            }
            std::vector<uint8_t> ogg;
            if (VorbisToOgg(*_stream, entry.Offset, _srcLen[at], static_cast<int>(chans), static_cast<int>(freq),
                            _samples[at], setup, modes, false, &ogg) < 0)
            {
                LastError = "Invalid Vorbis data";
                return DUDI7_ERR_IO;
            }
            return ExtractBytes(ogg.data(), static_cast<int64_t>(ogg.size()), sink);
        }
        case KindWav:
            break;
        default:
            return Archive::Extract(index, sink);
        }
        const int64_t len = entry.Size - WavHeader;
        int64_t fmt = 1, bits;
        switch (entry.DataX)
        {
        case 1:
            bits = 8;
            break;
        case 2:
            bits = 16;
            break;
        case 3:
            bits = 24;
            break;
        case 4:
            bits = 32;
            break;
        default:
            bits = 32;
            fmt = 3;
            break;
        }
        uint8_t hdr[44] = {};
        const auto put32 = [&](size_t p, uint32_t v) { std::memcpy(hdr + p, &v, 4); };
        std::memcpy(hdr, "RIFF", 4);
        put32(4, static_cast<uint32_t>(len + 36));
        std::memcpy(hdr + 8, "WAVEfmt ", 8);
        put32(16, 16);
        hdr[20] = static_cast<uint8_t>(fmt);
        hdr[22] = static_cast<uint8_t>(chans);
        put32(24, static_cast<uint32_t>(freq));
        put32(28, static_cast<uint32_t>(freq * chans * (bits / 8)));
        hdr[32] = static_cast<uint8_t>(chans * (bits / 8));
        hdr[34] = static_cast<uint8_t>(bits);
        std::memcpy(hdr + 36, "data", 4);
        put32(40, static_cast<uint32_t>(len));
        int32_t result = ExtractBytes(hdr, WavHeader, sink);
        if (result != DUDI7_OK)
            return result;
        Entry &mutableEntry = _entries[at];
        const int64_t saved = mutableEntry.Size;
        mutableEntry.Size = len;
        try
        {
            result = Archive::Extract(index, sink);
        }
        catch (...)
        {
            mutableEntry.Size = saved;
            throw;
        }
        mutableEntry.Size = saved;
        return result;
    }

  protected:
    void AddSound(const std::string &name, int64_t offset, int64_t size, int codec, int rate, int chans, uint8_t kind,
                  int64_t samples, uint32_t crc, uint8_t pad, int64_t srcLen)
    {
        Add(name, offset, size, codec, (static_cast<int64_t>(rate) << 8) | (chans & 255));
        _kind.push_back(kind);
        _samples.push_back(samples);
        _crc.push_back(crc);
        _pad.push_back(pad);
        _srcLen.push_back(srcLen);
    }

    // Walks the frames of an MPEG sound (the frames of FMOD are padded: to 4 bytes for Layer III, to 16 for
    // more than two channels); writes them one after the other when sink is given. total = their bytes.
    auto MpegFrames(int64_t start, int64_t len, int pad, const Sink *sink, int64_t &total) -> int32_t
    {
        int32_t result = DUDI7_ERR_NOTFORMAT;
        total = 0;
        int64_t pos = start;
        const int64_t endPos = start + len;
        while (pos + 4 <= endPos)
        {
            _stream->Seek(pos);
            uint8_t h[4];
            if (_stream->Read(h, 4) != 4)
                break;
            int layer;
            const int l = MpegFrameLen((static_cast<uint32_t>(h[0]) << 24) | (static_cast<uint32_t>(h[1]) << 16) |
                                           (static_cast<uint32_t>(h[2]) << 8) | h[3],
                                       layer);
            if (l < 4 || pos + l > endPos)
                break;
            if (sink != nullptr)
            {
                std::vector<uint8_t> buf(static_cast<size_t>(l));
                _stream->Seek(pos);
                if (_stream->Read(buf.data(), buf.size()) != buf.size())
                    return DUDI7_ERR_IO;
                result = ExtractBytes(buf.data(), l, *sink);
                if (result != DUDI7_OK)
                    return result;
            }
            total += l;
            int pd = 0;
            if (pad > 0 && (layer == 3 || pad == 16) && (l % pad) != 0)
                pd = pad - (l % pad);
            pos += l + pd;
            result = DUDI7_OK;
        }
        return result;
    }

    auto ReadBlock(int64_t pos, int64_t len, std::vector<uint8_t> &buf) -> bool
    {
        if (len < 0 || pos < 0 || pos + len > _stream->Size())
            return false;
        buf.assign(static_cast<size_t>(len) + 8, 0);
        if (len > 0)
        {
            _stream->Seek(pos);
            if (_stream->Read(buf.data(), static_cast<size_t>(len)) != static_cast<size_t>(len))
                return false;
        }
        return true;
    }

    // The FSB5 at base (limit = end of the container)
    void ParseFsb(int64_t base, int64_t limit)
    {
        uint8_t h[28];
        _stream->Seek(base);
        _stream->ReadBuffer(h, 28);
        const uint32_t ver = Le32(h, 4), ns = Le32(h, 8), shs = Le32(h, 12), nts = Le32(h, 16), ds = Le32(h, 20),
                       mode = Le32(h, 24);
        const uint32_t headerLength = ver == 1 ? 60 : 56;
        const int64_t dataStart = base + headerLength + shs + nts;
        if (dataStart > limit || dataStart + ds > limit)
            Fail(DUDI7_ERR_NOTFORMAT, "Truncated FSB");
        std::vector<uint8_t> heads, nameTab;
        if (!ReadBlock(base + headerLength, shs, heads))
            Fail(DUDI7_ERR_IO, "Cannot read the sample headers");
        if (!ReadBlock(base + headerLength + shs, nts, nameTab))
            Fail(DUDI7_ERR_IO, "Cannot read the names");
        const int64_t cnt = ns;
        std::vector<int64_t> offs(static_cast<size_t>(cnt) + 1), frs(static_cast<size_t>(cnt)),
            chs(static_cast<size_t>(cnt)), smp(static_cast<size_t>(cnt));
        std::vector<uint32_t> crcs(static_cast<size_t>(cnt));
        std::vector<bool> hasCrc(static_cast<size_t>(cnt)), layers(static_cast<size_t>(cnt));
        int64_t p = 0;
        for (int64_t i = 0; i < cnt; ++i)
        {
            const auto at = static_cast<size_t>(i);
            if (p + 8 > shs)
                Fail(DUDI7_ERR_NOTFORMAT, "Sample headers too short");
            const uint64_t bits = static_cast<uint64_t>(Le32(heads.data(), p)) | (static_cast<uint64_t>(Le32(heads.data(), p + 4)) << 32);
            p += 8;
            int64_t ex = static_cast<int64_t>(bits & 1);
            const int64_t freq = static_cast<int64_t>((bits >> 1) & 15);
            int64_t chans;
            switch ((bits >> 5) & 3)
            {
            case 0:
                chans = 1;
                break;
            case 1:
                chans = 2;
                break;
            case 2:
                chans = 6;
                break;
            default:
                chans = 8;
                break;
            }
            smp[at] = static_cast<int64_t>(bits >> 34);
            offs[at] = static_cast<int64_t>((bits >> 7) & 0x7FFFFFF) * 32;
            frs[at] = freq <= 9 ? Freqs[freq] : 44100;
            chs[at] = chans;
            while (ex != 0)
            {
                if (p + 4 > shs)
                    Fail(DUDI7_ERR_NOTFORMAT, "Sample headers too short");
                const uint32_t chunk = Le32(heads.data(), p);
                ex = chunk & 1;
                const int64_t chSize = (chunk >> 1) & 0xFFFFFF;
                const int64_t chType = chunk >> 25;
                p += 4;
                if (p + chSize > shs)
                    Fail(DUDI7_ERR_NOTFORMAT, "Sample headers too short");
                if (chType == 2 && chSize >= 4)
                    frs[at] = Le32(heads.data(), p);
                if (chType == 1 && chSize >= 1)
                    chs[at] = heads[static_cast<size_t>(p)];
                if (chType == 11 && chSize >= 4)
                {
                    crcs[at] = Le32(heads.data(), p);
                    hasCrc[at] = true;
                }
                if (chType == 14)
                    layers[at] = true; // Vorbis with several layers
                p += chSize;
            }
        }
        offs[static_cast<size_t>(cnt)] = ds;
        // the samples are not always stored in the order of the headers: a sample ends where the next one
        // (by position) starts
        std::vector<int64_t> sorted = offs;
        std::sort(sorted.begin(), sorted.end());
        const int codec = static_cast<int>(mode);
        _formatId = "FSB5";
        _dirSeparator = "/";
        std::set<std::string> used;
        for (int64_t i = 0; i < cnt; ++i)
        {
            const auto at = static_cast<size_t>(i);
            int64_t lo = 0, hi = cnt;
            while (lo < hi) // first sorted position after this sample's
            {
                const int64_t j = (lo + hi) / 2;
                if (sorted[static_cast<size_t>(j)] > offs[at])
                    hi = j;
                else
                    lo = j + 1;
            }
            const int64_t nxt = lo > cnt ? static_cast<int64_t>(ds) : sorted[static_cast<size_t>(lo)];
            if (offs[at] > ds || nxt > ds)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid sample position");
            int64_t size = nxt - offs[at];
            std::string name;
            if (nts >= static_cast<uint64_t>(i + 1) * 4)
            {
                const int64_t nameOff = Le32(nameTab.data(), i * 4);
                int64_t ne = nameOff;
                while (ne < nts && nameTab[static_cast<size_t>(ne)] != 0 && ne - nameOff < 255)
                    ++ne;
                if (nameOff < nts && ne > nameOff)
                    name.assign(reinterpret_cast<const char *>(&nameTab[static_cast<size_t>(nameOff)]),
                                static_cast<size_t>(ne - nameOff));
            }
            if (name.empty())
            {
                char number[32];
                std::snprintf(number, sizeof number, "sample_%.5lld", static_cast<long long>(i));
                name = number;
            }
            const int64_t srcSize = size;
            uint8_t kind = KindRaw;
            uint8_t pad = 0;
            std::string ext;
            if (codec >= 1 && codec <= 5)
                ext = ".wav";
            else if (codec == 11)
                ext = ".mp3";
            else
                ext = std::string(".") + CodecNames[codec] + ".bin";
            if (codec >= 1 && codec <= 5)
            {
                // the sound has exactly Samples x channels values: what follows is the padding of the format
                int64_t need = codec == 1 ? 1 : codec == 2 ? 2 : codec == 3 ? 3 : 4;
                need = need * chs[at] * smp[at];
                if (smp[at] > 0 && need <= size)
                    size = need;
                kind = KindWav;
            }
            else if (codec == 11 && chs[at] > 2)
                ext = ".mpeg.bin"; // several interleaved streams (one frame of each in turn): not a playable file
            else if (codec == 11)
            {
                pad = 4;
                int64_t need;
                if (MpegFrames(dataStart + offs[at], size, pad, nullptr, need) == DUDI7_OK)
                {
                    kind = KindMpeg;
                    size = need;
                }
                else
                    pad = 0;
            }
            else if (codec == 15 && hasCrc[at] && !layers[at])
            {
                const std::string setup = FsbSetupPacket(crcs[at]);
                VorbisModes modes;
                if (!setup.empty() && ParseVorbisSetup(setup, static_cast<int>(chs[at]), modes))
                {
                    const int64_t need = VorbisToOgg(*_stream, dataStart + offs[at], size, static_cast<int>(chs[at]),
                                                     static_cast<int>(frs[at]), smp[at], setup, modes, true, nullptr);
                    if (need > 0)
                    {
                        kind = KindOgg;
                        ext = ".ogg";
                        size = need;
                    }
                }
            }
            for (char &c : name)
                if (c == '/' || c == '\\')
                    c = '_';
            std::string key = ToLower(name + ext);
            if (used.count(key) > 0)
            {
                name += "_" + std::to_string(i);
                key = ToLower(name + ext);
            }
            used.insert(key);
            if (kind == KindWav)
                size += WavHeader;
            AddSound(name + ext, dataStart + offs[at], size, codec, static_cast<int>(frs[at]),
                     static_cast<int>(chs[at]), kind, smp[at], crcs[at], pad, srcSize);
        }
    }

    std::vector<uint8_t> _kind;
    std::vector<int64_t> _samples;
    std::vector<uint32_t> _crc;
    std::vector<uint8_t> _pad;    // MPEG: the frames are padded to a multiple of this
    std::vector<int64_t> _srcLen; // the bytes the sound has in the file (before it is converted)
};

class FsbArchive : public FsbBase
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return IsFsb5(stream, 0);
    }

    void Parse() override
    {
        ParseFsb(0, _stream->Size());
    }
};

class FmodBankArchive : public FsbBase
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint8_t h[12];
        return stream.Size() >= 12 && stream.Read(h, 12) == 12 && std::memcmp(h, "RIFF", 4) == 0 &&
               std::memcmp(h + 8, "FEV ", 4) == 0;
    }

    void Parse() override
    {
        _formatId = "FMODBANK";
        _dirSeparator = "/";
        int64_t pos = 12;
        while (pos + 8 <= _stream->Size())
        {
            _stream->Seek(pos);
            uint8_t c[8];
            _stream->ReadBuffer(c, 8);
            const int64_t size = Le32(c, 4);
            if (std::memcmp(c, "SND ", 4) == 0)
            {
                int64_t limit = pos + 8 + size;
                if (limit > _stream->Size())
                    limit = _stream->Size();
                // the FSB5 follows some padding
                uint8_t pad[64];
                const auto n = static_cast<int64_t>(_stream->Read(pad, sizeof pad));
                for (int64_t k = 0; k <= n - 4; ++k)
                    if (pad[k] == 'F' && pad[k + 1] == 'S' && pad[k + 2] == 'B' && pad[k + 3] == '5' &&
                        IsFsb5(*_stream, pos + 8 + k))
                    {
                        ParseFsb(pos + 8 + k, limit);
                        return;
                    }
                Fail(DUDI7_ERR_NOTFORMAT, "No FSB5 in the SND chunk");
            }
            pos += 8 + size + (size & 1);
        }
    }
};
} // namespace

void RegisterFsb()
{
    Register<FsbArchive>("*.FSB", "FMOD sound bank (*.FSB)");
}

void RegisterFmodBank()
{
    Register<FmodBankArchive>("*.BANK", "FMOD Studio bank (*.BANK)");
}
} // namespace Dud
