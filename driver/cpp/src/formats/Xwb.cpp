// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// XACT wave banks (.xwb, Xbox 360 / XNA / FNA games). Not in Dragon UnPACKer 5.
// Header: "WBND", version, header version, then 5 segments (offset, length): bank data, entry metadata,
// seek tables, entry names, wave data. Bank data: flags, number of entries, bank name[64], size of a
// metadata element, size of a name, alignment, compact format, build time. A metadata element of 24 bytes:
// flags + duration, format, play offset, play length, loop start, loop length. The format word holds the
// tag (2 bits: 0 PCM, 1 XMA, 2 ADPCM, 3 WMA), channels (3), sample rate (18), block align (8) and
// bits per sample (1). PCM and ADPCM become .wav files (the header is made here), XMA and WMA are
// extracted as they are (.xma / .wma: they have no header in the bank). The compact format (4 bytes
// per element) is not read.

#include <cstdio>
#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t WavPcmHeader = 44;
constexpr int64_t WavAdpcmHeader = 78;

auto Word(const uint8_t *buffer, size_t position) -> uint32_t
{
    uint32_t value;
    std::memcpy(&value, buffer + position, 4);
    return value;
}

class XwbArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint8_t h[52];
        if (stream.Size() < 52 || stream.Read(h, 52) != 52)
            return false;
        if (std::memcmp(h, "WBND", 4) != 0)
            return false;
        for (size_t i = 0; i < 5; ++i)
        {
            const int64_t off = Word(h, 12 + i * 8);
            const int64_t len = Word(h, 16 + i * 8);
            if (off + len > stream.Size())
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _formatId = "XWB";
        _dirSeparator = "";
        _stream->Seek(0);
        uint8_t h[52];
        _stream->ReadBuffer(h, 52);
        const int64_t bankOff = Word(h, 12);
        const int64_t bankLen = Word(h, 16);
        const int64_t metaOff = Word(h, 20);
        const int64_t metaLen = Word(h, 24);
        const int64_t dataOff = Word(h, 44);
        const int64_t dataLen = Word(h, 48);
        if (bankLen < 88)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid bank data");
        _stream->Seek(bankOff);
        uint8_t bank[88];
        _stream->ReadBuffer(bank, 88);
        const int64_t count = Word(bank, 4);
        const int64_t elemSize = Word(bank, 72);
        if (elemSize != 24)
            Fail(DUDI7_ERR_UNSUPPORTED, "Compact wave banks are not supported");
        CheckTable(metaOff, count, 24);
        if (count * 24 > metaLen)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid metadata");
        int digits = static_cast<int>(std::to_string(count).size());
        if (digits < 3)
            digits = 3;
        for (int64_t i = 0; i < count; ++i)
        {
            _stream->Seek(metaOff + i * 24);
            uint8_t e[24];
            _stream->ReadBuffer(e, 24);
            const uint32_t fmt = Word(e, 4);
            const int64_t playOff = Word(e, 8);
            const int64_t playLen = Word(e, 12);
            if (playOff + playLen > dataLen)
                Fail(DUDI7_ERR_NOTFORMAT, "Wave outside of the wave data");
            const uint32_t tag = fmt & 3;
            const char *ext = tag == 0 ? ".wav" : tag == 1 ? ".xma" : tag == 2 ? ".wav" : ".wma";
            char number[32];
            std::snprintf(number, sizeof number, "%0*lld", digits, static_cast<long long>(i));
            const std::string name = std::string("wave_") + number + ext;
            // DataX = the format word (the header of the .wav is made from it); the size has the header in front
            if (tag == 0)
                Add(name, dataOff + playOff, playLen + WavPcmHeader, fmt, i);
            else if (tag == 2)
                Add(name, dataOff + playOff, playLen + WavAdpcmHeader, fmt, i);
            else
                Add(name, dataOff + playOff, playLen, fmt, i);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        static const int16_t coef[14] = {256, 0, 512, -256, 0, 0, 192, 64, 240, 0, 460, -208, 392, -232};
        Entry &entry = _entries[static_cast<size_t>(index)];
        const uint32_t fmt = static_cast<uint32_t>(entry.DataX);
        const uint32_t tag = fmt & 3;
        const int chans = static_cast<int>((fmt >> 2) & 7);
        const int rate = static_cast<int>((fmt >> 5) & 0x3FFFF);
        const int align = static_cast<int>((fmt >> 23) & 0xFF);
        if (tag == 1 || tag == 3)
            return Archive::Extract(index, sink);
        if (chans == 0 || rate == 0)
        {
            LastError = "Invalid wave format";
            return DUDI7_ERR_IO;
        }
        uint8_t hdr[78] = {};
        const auto put32 = [&](size_t p, uint32_t v) { std::memcpy(hdr + p, &v, 4); };
        const auto put16 = [&](size_t p, int v) {
            hdr[p] = static_cast<uint8_t>(v & 255);
            hdr[p + 1] = static_cast<uint8_t>((v >> 8) & 255);
        };
        std::memcpy(hdr, "RIFF", 4);
        std::memcpy(hdr + 8, "WAVEfmt ", 8);
        int64_t headerLength;
        int64_t length;
        if (tag == 0)
        {
            headerLength = WavPcmHeader;
            const int bps = 8 + 8 * static_cast<int>(fmt >> 31);
            length = entry.Size - headerLength;
            put32(4, static_cast<uint32_t>(length + 36));
            put32(16, 16);
            put16(20, 1);
            put16(22, chans);
            put32(24, static_cast<uint32_t>(rate));
            put32(28, static_cast<uint32_t>(rate * chans * (bps / 8)));
            put16(32, chans * (bps / 8));
            put16(34, bps);
            std::memcpy(hdr + 36, "data", 4);
            put32(40, static_cast<uint32_t>(length));
        }
        else
        {
            // Microsoft ADPCM: the block align is stored minus 22 per channel
            headerLength = WavAdpcmHeader;
            const int blockAlign = (align + 22) * chans;
            const int spb = (blockAlign - 7 * chans) * 2 / chans + 2;
            length = entry.Size - headerLength;
            put32(4, static_cast<uint32_t>(length + 70));
            put32(16, 50);
            put16(20, 2);
            put16(22, chans);
            put32(24, static_cast<uint32_t>(rate));
            put32(28, static_cast<uint32_t>((static_cast<int64_t>(rate) * blockAlign) / spb));
            put16(32, blockAlign);
            put16(34, 4);
            put16(36, 32);
            put16(38, spb);
            put16(40, 7);
            for (int i = 0; i < 14; ++i)
                put16(42 + static_cast<size_t>(i) * 2, coef[i]);
            std::memcpy(hdr + 70, "data", 4);
            put32(74, static_cast<uint32_t>(length));
        }
        int32_t result = ExtractBytes(hdr, headerLength, sink);
        if (result != DUDI7_OK)
            return result;
        const int64_t saved = entry.Size;
        entry.Size = length;
        try
        {
            result = Archive::Extract(index, sink);
        }
        catch (...)
        {
            entry.Size = saved;
            throw;
        }
        entry.Size = saved;
        return result;
    }
};
} // namespace

void RegisterXwb()
{
    Register<XwbArchive>("*.XWB", "XACT wave bank (*.XWB)");
}
} // namespace Dud
