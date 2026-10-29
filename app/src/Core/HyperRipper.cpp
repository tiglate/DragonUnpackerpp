// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/HyperRipper.h"

#include "Core/RipNaming.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace Dup::Core
{
namespace
{
constexpr std::size_t MIN_BLOCK = 4U * 1024;
constexpr std::size_t MAX_BLOCK = 256U * 1024 * 1024;
constexpr std::size_t MAX_MAGIC = 32;

/// A reader with a cache window so that the finders can read a few bytes at a time.
class CachedReader
{
  public:
    explicit CachedReader(const RipReader &source) : _source(source), _window(256U * 1024)
    {
    }
    [[nodiscard]] auto Size() const -> std::int64_t
    {
        return _source.Size();
    }
    /// Bytes at offset, false when they are not all inside the file.
    auto Get(std::int64_t offset, void *buffer, std::size_t length) -> bool
    {
        if (offset < 0 || offset + static_cast<std::int64_t>(length) > _source.Size())
        {
            return false;
        }
        if (length > _window.size())
        {
            return _source.Read(offset, buffer, length) == length;
        }
        if (offset < _start || offset + static_cast<std::int64_t>(length) > _start + static_cast<std::int64_t>(_filled))
        {
            _start = offset;
            _filled = _source.Read(offset, _window.data(), _window.size());
            if (offset + static_cast<std::int64_t>(length) > _start + static_cast<std::int64_t>(_filled))
            {
                return false;
            }
        }
        std::memcpy(buffer, _window.data() + (offset - _start), length);
        return true;
    }
    auto U8(std::int64_t offset) -> int
    {
        std::uint8_t v = 0;
        return Get(offset, &v, 1) ? v : -1;
    }
    auto U16Le(std::int64_t offset) -> std::int64_t
    {
        std::uint8_t b[2];
        return Get(offset, b, 2) ? (b[0] | (b[1] << 8)) : -1;
    }
    auto U16Be(std::int64_t offset) -> std::int64_t
    {
        std::uint8_t b[2];
        return Get(offset, b, 2) ? ((b[0] << 8) | b[1]) : -1;
    }
    auto U32Le(std::int64_t offset) -> std::int64_t
    {
        std::uint8_t b[4];
        return Get(offset, b, 4) ? (static_cast<std::int64_t>(b[0]) | (b[1] << 8) | (b[2] << 16) | (static_cast<std::int64_t>(b[3]) << 24)) : -1;
    }
    auto U32Be(std::int64_t offset) -> std::int64_t
    {
        std::uint8_t b[4];
        return Get(offset, b, 4) ? ((static_cast<std::int64_t>(b[0]) << 24) | (b[1] << 16) | (b[2] << 8) | b[3]) : -1;
    }
    auto Matches(std::int64_t offset, const char *text) -> bool
    {
        const std::size_t length = std::strlen(text);
        std::array<char, MAX_MAGIC> buffer{};
        return length <= buffer.size() && Get(offset, buffer.data(), length) && std::memcmp(buffer.data(), text, length) == 0;
    }

  private:
    const RipReader &_source;
    std::vector<std::uint8_t> _window;
    std::int64_t _start{-1};
    std::size_t _filled{0};
};

struct Found
{
    std::int64_t Size{0};
    std::string Extension;
};

using Finder = std::optional<Found> (*)(CachedReader &, std::int64_t, const HyperRipper::Options &);

struct Signature
{
    const char *Type;
    const char *Extension;
    const char *Description;
    std::string Magic;
    Finder Find;
    std::int64_t MagicBack{0}; // the magic number is this many bytes after the start of the file
};

auto FindPng(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    std::int64_t position = start + 8;
    for (int chunks = 0; chunks < 100000; ++chunks)
    {
        const auto length = r.U32Be(position);
        if (length < 0 || length > 0x7fffffff)
        {
            return std::nullopt;
        }
        std::array<char, 4> type{};
        if (!r.Get(position + 4, type.data(), 4))
        {
            return std::nullopt;
        }
        for (const char c : type)
        {
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
            {
                return std::nullopt;
            }
        }
        if (chunks == 0 && (std::memcmp(type.data(), "IHDR", 4) != 0 || length != 13))
        {
            return std::nullopt;
        }
        position += 12 + length;
        if (position > r.Size())
        {
            return std::nullopt;
        }
        if (std::memcmp(type.data(), "IEND", 4) == 0)
        {
            return Found{position - start, "png"};
        }
    }
    return std::nullopt;
}

auto FindJpeg(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    std::int64_t position = start + 2;
    bool first = true;
    bool sawFrame = false;
    while (position + 2 <= r.Size())
    {
        int marker = r.U8(position);
        if (marker != 0xFF)
        {
            return std::nullopt;
        }
        while (r.U8(position + 1) == 0xFF) // fill bytes
        {
            ++position;
        }
        marker = r.U8(position + 1);
        position += 2;
        if (marker == 0xD9)
        {
            return first ? std::nullopt : std::optional<Found>(Found{position - start, "jpg"});
        }
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7))
        {
            continue;
        }
        if (first)
        {
            // the first segment of a real file: JFIF, Exif or Adobe application data, or the quantization tables
            const bool known = (marker == 0xE0 && (r.Matches(position + 2, "JFIF") || r.Matches(position + 2, "JFXX"))) ||
                               (marker == 0xE1 && (r.Matches(position + 2, "Exif") || r.Matches(position + 2, "http"))) ||
                               (marker == 0xEE && r.Matches(position + 2, "Adobe")) || marker == 0xDB;
            if (!known)
            {
                return std::nullopt;
            }
        }
        first = false;
        // only the markers of real files: APPn, COM, DQT, DHT, DRI, SOFn and SOS (random data gives others, or silly lengths)
        const bool sof = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (!(sof || marker == 0xC4 || marker == 0xDB || marker == 0xDD || marker == 0xDA || marker == 0xFE || (marker >= 0xE0 && marker <= 0xEF)))
        {
            return std::nullopt;
        }
        sawFrame = sawFrame || sof;
        if (marker == 0xDA && !sawFrame)
        {
            return std::nullopt;
        }
        const auto length = r.U16Be(position);
        if (length < 2 || position + length > r.Size())
        {
            return std::nullopt;
        }
        position += length;
        if (marker == 0xDA)
        {
            // entropy coded data: up to the next marker that is not a restart or a stuffed zero
            while (position + 1 < r.Size())
            {
                if (r.U8(position) == 0xFF)
                {
                    const int next = r.U8(position + 1);
                    if (next != 0x00 && !(next >= 0xD0 && next <= 0xD7) && next != 0xFF)
                    {
                        break;
                    }
                }
                ++position;
            }
        }
    }
    return std::nullopt;
}

auto SkipSubBlocks(CachedReader &r, std::int64_t &position) -> bool
{
    while (true)
    {
        const int length = r.U8(position);
        if (length < 0)
        {
            return false;
        }
        ++position;
        if (length == 0)
        {
            return true;
        }
        position += length;
    }
}

auto FindGif(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const int packed = r.U8(start + 10);
    if (packed < 0)
    {
        return std::nullopt;
    }
    std::int64_t position = start + 13 + ((packed & 0x80) != 0 ? 3 * (1 << ((packed & 7) + 1)) : 0);
    for (int blocks = 0; blocks < 1000000; ++blocks)
    {
        const int id = r.U8(position);
        if (id == 0x3B)
        {
            return Found{position + 1 - start, "gif"};
        }
        if (id == 0x21)
        {
            position += 2;
            if (!SkipSubBlocks(r, position))
            {
                return std::nullopt;
            }
        }
        else if (id == 0x2C)
        {
            const int local = r.U8(position + 9);
            if (local < 0)
            {
                return std::nullopt;
            }
            position += 10 + ((local & 0x80) != 0 ? 3 * (1 << ((local & 7) + 1)) : 0) + 1; // + LZW minimum code size
            if (!SkipSubBlocks(r, position))
            {
                return std::nullopt;
            }
        }
        else
        {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

auto FindBmp(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const auto size = r.U32Le(start + 2);
    const auto reserved = r.U32Le(start + 6);
    const auto dataOffset = r.U32Le(start + 10);
    const auto header = r.U32Le(start + 14);
    const bool knownHeader = header == 12 || header == 40 || header == 52 || header == 56 || header == 64 || header == 108 || header == 124;
    if (size < 26 || reserved != 0 || !knownHeader || dataOffset < 14 + header || dataOffset > size || start + size > r.Size())
    {
        return std::nullopt;
    }
    return Found{size, "bmp"};
}

auto FindRiff(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const auto size = r.U32Le(start + 4) + 8;
    if (size < 12 || start + size > r.Size())
    {
        return std::nullopt;
    }
    std::string extension = "riff";
    if (r.Matches(start + 8, "WAVE"))
    {
        extension = "wav";
    }
    else if (r.Matches(start + 8, "AVI "))
    {
        extension = "avi";
    }
    else if (r.Matches(start + 8, "WEBP"))
    {
        extension = "webp";
    }
    else if (r.Matches(start + 8, "RMID"))
    {
        extension = "mid";
    }
    else
    {
        return std::nullopt; // other RIFF forms (ANI, CDA...) are too uncommon to be worth the false positives
    }
    return Found{size, extension};
}

auto FindOgg(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    if (r.U8(start + 4) != 0 || (r.U8(start + 5) & 2) == 0) // version 0, first page of a stream
    {
        return std::nullopt;
    }
    std::int64_t position = start;
    for (int pages = 0; pages < 20000000; ++pages)
    {
        if (!r.Matches(position, "OggS"))
        {
            return std::nullopt;
        }
        const int flags = r.U8(position + 5);
        const int segments = r.U8(position + 26);
        if (flags < 0 || segments < 0)
        {
            return std::nullopt;
        }
        std::int64_t body = 0;
        for (int i = 0; i < segments; ++i)
        {
            body += r.U8(position + 27 + i);
        }
        position += 27 + segments + body;
        if (position > r.Size())
        {
            return std::nullopt;
        }
        if ((flags & 4) != 0)
        {
            return Found{position - start, "ogg"};
        }
    }
    return std::nullopt;
}

auto FindZip(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const auto method = r.U16Le(start + 8);
    const auto version = r.U16Le(start + 4);
    if (version < 0 || version > 100 || (method != 0 && method != 8 && method != 9 && method != 12 && method != 14 && method != 93 && method != 95 && method != 98))
    {
        return std::nullopt;
    }
    // the end of central directory record "PK\5\6": scan forward
    std::vector<std::uint8_t> block(64 * 1024);
    for (std::int64_t position = start; position + 22 <= r.Size(); position += static_cast<std::int64_t>(block.size()) - 4)
    {
        const std::size_t want = static_cast<std::size_t>(std::min<std::int64_t>(static_cast<std::int64_t>(block.size()), r.Size() - position));
        if (!r.Get(position, block.data(), want))
        {
            break;
        }
        for (std::size_t i = 0; i + 22 <= want; ++i)
        {
            if (block[i] == 'P' && block[i + 1] == 'K' && block[i + 2] == 5 && block[i + 3] == 6)
            {
                const std::int64_t comment = block[i + 20] | (block[i + 21] << 8);
                const std::int64_t end = position + static_cast<std::int64_t>(i) + 22 + comment;
                if (end <= r.Size())
                {
                    return Found{end - start, "zip"};
                }
            }
        }
        if (position - start > (2LL << 30))
        {
            break;
        }
    }
    return std::nullopt;
}

auto MipBytes(std::int64_t width, std::int64_t height, std::int64_t blockBytes, std::int64_t bitsPerPixel) -> std::int64_t
{
    if (blockBytes > 0)
    {
        return ((width + 3) / 4) * ((height + 3) / 4) * blockBytes;
    }
    return width * height * bitsPerPixel / 8;
}

auto FindDds(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    if (r.U32Le(start + 4) != 124)
    {
        return std::nullopt;
    }
    const auto height = r.U32Le(start + 12);
    const auto width = r.U32Le(start + 16);
    auto mips = r.U32Le(start + 28);
    const auto pixelFlags = r.U32Le(start + 80);
    const auto bpp = r.U32Le(start + 88);
    const auto caps2 = r.U32Le(start + 112);
    if (width < 1 || height < 1 || width > 32768 || height > 32768)
    {
        return std::nullopt;
    }
    std::int64_t header = 128;
    std::int64_t blockBytes = 0;
    std::int64_t bits = bpp;
    if ((pixelFlags & 4) != 0)
    {
        if (r.Matches(start + 84, "DXT1") || r.Matches(start + 84, "BC4U") || r.Matches(start + 84, "ATI1"))
        {
            blockBytes = 8;
        }
        else if (r.Matches(start + 84, "DXT3") || r.Matches(start + 84, "DXT5") || r.Matches(start + 84, "BC5U") || r.Matches(start + 84, "ATI2"))
        {
            blockBytes = 16;
        }
        else if (r.Matches(start + 84, "DX10"))
        {
            header = 148;
            const auto dxgi = r.U32Le(start + 128);
            blockBytes = (dxgi >= 70 && dxgi <= 72) || (dxgi >= 79 && dxgi <= 81) ? 8 : (dxgi >= 73 && dxgi <= 78) || (dxgi >= 82 && dxgi <= 84) || (dxgi >= 94 && dxgi <= 99) ? 16 : 0;
            if (blockBytes == 0)
            {
                bits = 32; // the common uncompressed formats
            }
        }
        else
        {
            return std::nullopt;
        }
    }
    else if (bits == 0)
    {
        return std::nullopt;
    }
    if (mips < 1)
    {
        mips = 1;
    }
    std::int64_t total = header;
    std::int64_t w = width;
    std::int64_t h = height;
    for (std::int64_t level = 0; level < std::min<std::int64_t>(mips, 16); ++level)
    {
        total += MipBytes(w, h, blockBytes, bits);
        w = std::max<std::int64_t>(1, w / 2);
        h = std::max<std::int64_t>(1, h / 2);
    }
    if ((caps2 & 0x200) != 0)
    {
        total = header + (total - header) * 6; // a cube map: six faces
    }
    if (total > r.Size() - start)
    {
        return std::nullopt;
    }
    return Found{total, "dds"};
}

auto FindMidi(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    if (r.U32Be(start + 4) != 6)
    {
        return std::nullopt;
    }
    const auto tracks = r.U16Be(start + 10);
    if (tracks < 1 || tracks > 256)
    {
        return std::nullopt;
    }
    std::int64_t position = start + 14;
    for (std::int64_t t = 0; t < tracks; ++t)
    {
        if (!r.Matches(position, "MTrk"))
        {
            return std::nullopt;
        }
        const auto length = r.U32Be(position + 4);
        if (length < 0)
        {
            return std::nullopt;
        }
        position += 8 + length;
        if (position > r.Size())
        {
            return std::nullopt;
        }
    }
    return Found{position - start, "mid"};
}

// ---------------------------------------------------------------------------------------------------------------------
// Formats of the v5 HyperRipper that were missing: tracker modules, VOC, MP3, AAC, MP4/MOV, Bink, FLIC, Matroska,
// EMF, WMF, LBM, TGA. Each finder gets the start of the file (the magic number may be inside the file: MagicBack).
// ---------------------------------------------------------------------------------------------------------------------

auto FindVoc(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const auto headerSize = r.U16Le(start + 20);
    if (r.U8(start + 19) != 26 || headerSize < 26 || r.U16Le(start + 22) < 0x0100)
    {
        return std::nullopt;
    }
    std::int64_t position = start + headerSize;
    for (int blocks = 0; blocks < 1000000; ++blocks)
    {
        const int type = r.U8(position);
        if (type < 0 || type > 9)
        {
            return std::nullopt;
        }
        if (type == 0)
        {
            return Found{position + 1 - start, "voc"};
        }
        const auto b0 = r.U8(position + 1);
        const auto b1 = r.U8(position + 2);
        const auto b2 = r.U8(position + 3);
        if (b2 < 0)
        {
            return std::nullopt;
        }
        position += 4 + (b0 | (b1 << 8) | (b2 << 16));
        if (position > r.Size())
        {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

auto FindModule669(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const int samples = r.U8(start + 110);
    const int patterns = r.U8(start + 111);
    if (samples < 1 || samples > 64 || patterns < 1 || patterns > 128 || r.U8(start + 112) >= 128)
    {
        return std::nullopt;
    }
    std::array<std::uint8_t, 108> message{}; // text of the song
    if (!r.Get(start + 2, message.data(), message.size()))
    {
        return std::nullopt;
    }
    for (const auto c : message)
    {
        if (c != 0 && (c < 32 || c > 126))
        {
            return std::nullopt;
        }
    }
    std::int64_t size = 0x1F1 + samples * 0x19LL + patterns * 0x600LL;
    for (int s = 0; s < samples; ++s)
    {
        const auto length = r.U32Le(start + 0x1F1 + s * 25LL + 13);
        // (the sample headers follow the 497 bytes of the header)
        if (length < 0)
        {
            return std::nullopt;
        }
        size += length;
        if (start + size > r.Size())
        {
            return std::nullopt;
        }
    }
    return Found{size, "669"};
}

auto FindXm(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    if (!r.Matches(start, "Extended Module: ") || r.U8(start + 37) != 0x1A)
    {
        return std::nullopt;
    }
    const auto headerSize = r.U32Le(start + 60);
    const auto patterns = r.U16Le(start + 70);
    const auto instruments = r.U16Le(start + 72);
    if (headerSize < 20 || headerSize > 1024 || patterns < 0 || patterns > 256 || instruments < 0 || instruments > 128)
    {
        return std::nullopt;
    }
    std::int64_t size = headerSize + 60;
    for (int p = 0; p < patterns; ++p)
    {
        const auto length = r.U32Le(start + size);
        const auto data = r.U16Le(start + size + 7);
        if (length < 0 || data < 0)
        {
            return std::nullopt;
        }
        size += length + data;
    }
    for (int i = 0; i < instruments; ++i)
    {
        const auto headerLength = r.U32Le(start + size);
        const auto samples = r.U16Le(start + size + 27);
        if (headerLength < 29 || samples < 0 || samples > 256)
        {
            return std::nullopt;
        }
        std::int64_t at = size + headerLength;
        if (samples > 0)
        {
            const auto sampleHeader = r.U32Le(start + size + 29);
            if (sampleHeader < 0)
            {
                return std::nullopt;
            }
            at = size + headerLength;
            std::int64_t data = 0;
            for (int s = 0; s < samples; ++s)
            {
                const auto length = r.U32Le(start + at + s * sampleHeader);
                if (length < 0)
                {
                    return std::nullopt;
                }
                data += length;
            }
            at += samples * sampleHeader + data;
        }
        size = at;
        if (start + size > r.Size())
        {
            return std::nullopt;
        }
    }
    return Found{size, "xm"};
}

auto FindS3m(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    if (r.U8(start + 28) != 0x1A || r.U8(start + 29) != 16)
    {
        return std::nullopt;
    }
    const auto orders = r.U16Le(start + 32);
    const auto instruments = r.U16Le(start + 34);
    const auto patterns = r.U16Le(start + 36);
    if (orders < 1 || orders > 256 || instruments < 0 || instruments > 255 || patterns < 1 || patterns > 255)
    {
        return std::nullopt;
    }
    std::int64_t size = 96 + orders + instruments * 2 + patterns * 2;
    const std::int64_t instrumentTable = 96 + orders;
    for (int i = 0; i < instruments; ++i)
    {
        const auto paragraph = r.U16Le(start + instrumentTable + i * 2);
        if (paragraph < 0)
        {
            return std::nullopt;
        }
        const auto at = start + paragraph * 16;
        const int type = r.U8(at);
        const auto length = r.U32Le(at + 16);
        if (type < 0 || length < 0)
        {
            return std::nullopt;
        }
        size += 80 + (type >= 2 ? 0 : (length + 15) / 16 * 16);
    }
    const std::int64_t patternTable = instrumentTable + instruments * 2;
    for (int p = 0; p < patterns; ++p)
    {
        const auto paragraph = r.U16Le(start + patternTable + p * 2);
        if (paragraph < 0)
        {
            return std::nullopt;
        }
        if (paragraph > 0)
        {
            const auto length = r.U16Le(start + paragraph * 16);
            if (length < 0)
            {
                return std::nullopt;
            }
            size += (length + 15) / 16 * 16 + 2;
        }
    }
    size = std::min(size, r.Size() - start);
    return Found{size, "s3m"};
}

auto FindIt(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const auto orders = r.U16Le(start + 32);
    const auto instruments = r.U16Le(start + 34);
    const auto samples = r.U16Le(start + 36);
    const auto patterns = r.U16Le(start + 38);
    if (orders < 1 || orders > 256 || instruments < 0 || instruments > 255 || samples < 0 || samples > 4000 || patterns < 0 || patterns > 256 || r.U16Le(start + 40) < 0x100)
    {
        return std::nullopt;
    }
    const std::int64_t tables = 192 + orders;
    std::int64_t size = tables + (instruments + samples + patterns) * 4;
    for (int i = 0; i < instruments; ++i)
    {
        const auto at = r.U32Le(start + tables + i * 4);
        if (at < 0)
        {
            return std::nullopt;
        }
        size = std::max<std::int64_t>(size, at + 554);
    }
    for (int s = 0; s < samples; ++s)
    {
        const auto header = r.U32Le(start + tables + instruments * 4LL + s * 4);
        if (header < 0 || !r.Matches(start + header, "IMPS"))
        {
            return std::nullopt;
        }
        const auto flags = r.U8(start + header + 18);
        const auto length = r.U32Le(start + header + 48);
        const auto pointer = r.U32Le(start + header + 72);
        if (flags < 0 || length < 0 || pointer < 0)
        {
            return std::nullopt;
        }
        std::int64_t bytes = length;
        if ((flags & 2) != 0)
        {
            bytes *= 2;
        }
        if ((flags & 4) != 0)
        {
            bytes *= 2;
        }
        size = std::max<std::int64_t>(size, pointer + bytes);
        size = std::max<std::int64_t>(size, header + 80);
    }
    for (int p = 0; p < patterns; ++p)
    {
        const auto at = r.U32Le(start + tables + (instruments + samples) * 4LL + p * 4);
        if (at < 0)
        {
            return std::nullopt;
        }
        if (at > 0)
        {
            const auto length = r.U16Le(start + at);
            if (length < 0)
            {
                return std::nullopt;
            }
            size = std::max<std::int64_t>(size, at + 8 + length);
        }
    }
    size = std::min(size, r.Size() - start);
    return Found{size, "it"};
}

constexpr int MP3_BITRATES[3][16] = {
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},   // MPEG-1 layer I
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},      // MPEG-1 layer II
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},       // MPEG-1 layer III
};
constexpr int MP3_BITRATES_V2[2][16] = {
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},      // MPEG-2/2.5 layer I
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},           // MPEG-2/2.5 layers II and III
};

/// One MPEG audio frame.
struct Mp3Frame
{
    std::int64_t Length{0};
    int Version{0};      // 0 = MPEG-1, 1 = MPEG-2, 2 = MPEG-2.5
    int Layer{0};        // 0 = I, 1 = II, 2 = III
    bool Mono{false};
    bool Crc{false};     // 16 bits of CRC after the header
};

/// The frame at offset, Length 0 when it is not one that the options allow; the same version, layer and rate as the first one.
auto Mp3ReadFrame(CachedReader &r, std::int64_t at, std::int64_t &signature, const MpegOptions &options) -> Mp3Frame
{
    const auto header = r.U32Be(at);
    if (header < 0 || ((header >> 21) & 0x7FF) != 0x7FF)
    {
        return {};
    }
    const int versionBits = static_cast<int>((header >> 19) & 3); // 3 = MPEG-1, 2 = MPEG-2, 0 = MPEG-2.5
    const int layerBits = static_cast<int>((header >> 17) & 3);   // 3 = layer I, 2 = II, 1 = III
    const int bitrateIndex = static_cast<int>((header >> 12) & 15);
    const int rateIndex = static_cast<int>((header >> 10) & 3);
    const int padding = static_cast<int>((header >> 9) & 1);
    if (versionBits == 1 || layerBits == 0 || bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3)
    {
        return {};
    }
    Mp3Frame frame;
    frame.Version = versionBits == 3 ? 0 : (versionBits == 2 ? 1 : 2);
    frame.Layer = 3 - layerBits;
    if (!options.Layers[frame.Version][frame.Layer])
    {
        return {};
    }
    const std::int64_t id = header & 0xFFFE0C00; // version, layer and rate must not change
    if (signature != 0 && id != signature)
    {
        return {};
    }
    signature = id;
    static constexpr int RATES[3] = {44100, 48000, 32000};
    int rate = RATES[rateIndex];
    if (frame.Version == 1)
    {
        rate /= 2;
    }
    else if (frame.Version == 2)
    {
        rate /= 4;
    }
    const int bitrate = (frame.Version == 0 ? MP3_BITRATES[frame.Layer][bitrateIndex] : MP3_BITRATES_V2[frame.Layer == 0 ? 0 : 1][bitrateIndex]) * 1000;
    if (frame.Layer == 0)
    {
        frame.Length = (12LL * bitrate / rate + padding) * 4;
    }
    else
    {
        const std::int64_t samples = (frame.Layer == 2 && frame.Version != 0) ? 72 : 144;
        frame.Length = samples * bitrate / rate + padding;
    }
    frame.Mono = ((header >> 6) & 3) == 3;
    frame.Crc = ((header >> 16) & 1) == 0;
    return frame;
}

/// The Xing/Info header of a VBR file: the size of the whole file and the number of frames, 0 when absent.
auto Mp3XingSize(CachedReader &r, std::int64_t at, const Mp3Frame &frame, std::int64_t &frames) -> std::int64_t
{
    const std::int64_t sideInfo = frame.Version == 0 ? (frame.Mono ? 17 : 32) : (frame.Mono ? 9 : 17);
    const std::int64_t tag = at + 4 + (frame.Crc ? 2 : 0) + sideInfo;
    if (!r.Matches(tag, "Xing") && !r.Matches(tag, "Info"))
    {
        return 0;
    }
    const auto flags = r.U32Be(tag + 4);
    if (flags < 0 || (flags & 2) == 0)
    {
        return 0;
    }
    std::int64_t position = tag + 8;
    frames = 0;
    if ((flags & 1) != 0)
    {
        frames = r.U32Be(position);
        position += 4;
    }
    const auto bytes = r.U32Be(position);
    return bytes > 0 ? bytes : 0;
}

auto FindMp3(CachedReader &r, std::int64_t start, const HyperRipper::Options &options) -> std::optional<Found>
{
    const MpegOptions &mpeg = options.Mpeg;
    std::int64_t signature = 0;
    std::int64_t position = start;
    std::int64_t frames = 0;
    if (r.Matches(start, "ID3"))
    {
        if (!mpeg.Id3Tag)
        {
            return std::nullopt;
        }
        // an ID3v2 tag in front: version, flags and a size of 4 x 7 bits
        std::array<std::uint8_t, 10> tag{};
        if (!r.Get(start, tag.data(), tag.size()) || tag[3] == 0xFF || tag[4] == 0xFF || ((tag[6] | tag[7] | tag[8] | tag[9]) & 0x80) != 0)
        {
            return std::nullopt;
        }
        position += 10 + ((tag[6] << 21) | (tag[7] << 14) | (tag[8] << 7) | tag[9]) + ((tag[5] & 0x10) != 0 ? 10 : 0);
    }
    const std::int64_t audioStart = position;
    std::int64_t xingSize = 0;
    bool first = true;
    while (true)
    {
        const Mp3Frame frame = Mp3ReadFrame(r, position, signature, mpeg);
        if (frame.Length < 24 || position + frame.Length > r.Size())
        {
            break;
        }
        if (first && mpeg.XingVbr)
        {
            std::int64_t counted = 0;
            xingSize = Mp3XingSize(r, position, frame, counted);
            if (xingSize > 0 && audioStart + xingSize <= r.Size())
            {
                frames = counted > 0 ? counted : (mpeg.LimitFramesMin ? mpeg.FramesMin : 4);
                position = audioStart + xingSize;
                break;
            }
            xingSize = 0;
        }
        first = false;
        position += frame.Length;
        ++frames;
        if ((mpeg.LimitFramesMax && frames >= mpeg.FramesMax) || (mpeg.LimitSizeMax && position - audioStart >= mpeg.SizeMax))
        {
            break;
        }
    }
    if (frames < (mpeg.LimitFramesMin ? static_cast<std::int64_t>(mpeg.FramesMin) : 4) || (mpeg.LimitSizeMin && position - start < mpeg.SizeMin))
    {
        return std::nullopt; // fewer frames are not told apart from noise
    }
    if (mpeg.Id3Tag && xingSize == 0 && r.Matches(position, "TAG") && position + 128 <= r.Size())
    {
        position += 128; // ID3v1
    }
    return Found{position - start, "mp3"};
}

auto FindAac(CachedReader &r, std::int64_t start, const HyperRipper::Options &options) -> std::optional<Found>
{
    const MpegOptions &mpeg = options.Mpeg;
    std::int64_t position = start;
    std::int64_t frames = 0;
    std::int64_t firstProfile = -1;
    while (true)
    {
        std::array<std::uint8_t, 7> h{};
        if (!r.Get(position, h.data(), h.size()))
        {
            break;
        }
        if (h[0] != 255 || (h[1] & 240) != 240 || (h[1] & 6) != 0 || (h[2] & 2) != 0 || (h[3] & 60) != 0 || (h[5] & 31) != 31 || (h[6] & 252) != 252 || (h[6] & 3) != 0)
        {
            break;
        }
        const std::int64_t profile = h[2] & 0xFC; // object type and rate
        const int rate = (h[2] >> 2) & 15;
        if (rate > 12 || (firstProfile >= 0 && profile != firstProfile))
        {
            break;
        }
        firstProfile = profile;
        const std::int64_t length = ((h[3] & 3) << 11) | (h[4] << 3) | (h[5] >> 5);
        if (length < 7 || position + length > r.Size())
        {
            break;
        }
        position += length;
        ++frames;
        if ((mpeg.LimitFramesMax && frames >= mpeg.FramesMax) || (mpeg.LimitSizeMax && position - start >= mpeg.SizeMax))
        {
            break;
        }
    }
    if (frames < (mpeg.LimitFramesMin ? static_cast<std::int64_t>(mpeg.FramesMin) : 4) || (mpeg.LimitSizeMin && position - start < mpeg.SizeMin))
    {
        return std::nullopt;
    }
    return Found{position - start, "aac"};
}

/// MP4, M4A and QuickTime movies: a chain of atoms with a size and a name.
auto FindIsoMedia(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    static constexpr const char *KNOWN[] = {"ftyp", "moov", "mdat", "free", "skip", "wide", "uuid", "pnot", "meta", "moof", "mfra", "sidx", "styp", "pdin", "udta"};
    std::int64_t position = start;
    bool movieData = false;
    bool first = true;
    std::string extension = "mp4";
    for (int atoms = 0; atoms < 100000; ++atoms)
    {
        std::int64_t size = r.U32Be(position);
        std::array<char, 4> type{};
        if (size < 0 || !r.Get(position + 4, type.data(), 4))
        {
            break;
        }
        bool known = false;
        for (const char *name : KNOWN)
        {
            known = known || std::memcmp(type.data(), name, 4) == 0;
        }
        if (!known || (first && std::memcmp(type.data(), "ftyp", 4) != 0 && std::memcmp(type.data(), "moov", 4) != 0))
        {
            break;
        }
        if (size == 1)
        {
            const auto high = r.U32Be(position + 8);
            const auto low = r.U32Be(position + 12);
            if (high < 0 || high > 0x7fff || low < 0)
            {
                break;
            }
            size = (high << 32) | low;
        }
        else if (size == 0)
        {
            size = r.Size() - position;
        }
        if (size < 8 || position + size > r.Size())
        {
            break;
        }
        if (first && std::memcmp(type.data(), "ftyp", 4) == 0)
        {
            std::array<char, 4> brand{};
            r.Get(position + 8, brand.data(), 4);
            if (std::memcmp(brand.data(), "M4A ", 4) == 0 || std::memcmp(brand.data(), "M4B ", 4) == 0 || std::memcmp(brand.data(), "M4P ", 4) == 0)
            {
                extension = "m4a";
            }
            else if (std::memcmp(brand.data(), "qt  ", 4) == 0)
            {
                extension = "mov";
            }
        }
        else if (first)
        {
            extension = "mov";
        }
        first = false;
        movieData = movieData || std::memcmp(type.data(), "mdat", 4) == 0;
        position += size;
    }
    if (first || !movieData)
    {
        return std::nullopt;
    }
    return Found{position - start, extension};
}

auto FindBink(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const int revision = r.U8(start + 3);
    if (revision != 'b' && revision != 'f' && revision != 'g' && revision != 'h' && revision != 'i' && revision != 'k')
    {
        return std::nullopt;
    }
    const auto size = r.U32Le(start + 4) + 8;
    const auto frames = r.U32Le(start + 8);
    const auto width = r.U32Le(start + 20);
    const auto height = r.U32Le(start + 24);
    if (size < 44 || frames < 1 || frames > 1000000 || width < 1 || width > 8192 || height < 1 || height > 8192 || start + size > r.Size())
    {
        return std::nullopt;
    }
    return Found{size, "bik"};
}

auto FindFlic(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const auto size = r.U32Le(start);
    const auto magic = r.U16Le(start + 4);
    const auto frames = r.U16Le(start + 6);
    const auto width = r.U16Le(start + 8);
    const auto height = r.U16Le(start + 10);
    const auto depth = r.U16Le(start + 12);
    if (size < 128 || start + size > r.Size() || r.U32Le(start + 14) != 0 || frames < 1 || frames > 4000 || width < 1 || width > 4096 || height < 1 || height > 4096 || depth != 8)
    {
        return std::nullopt;
    }
    return Found{size, magic == 0xAF11 ? "fli" : "flc"};
}

/// One EBML element header: the id and the size of the content, false for an unknown size or a bad header.
auto EbmlElement(CachedReader &r, std::int64_t position, std::int64_t &id, std::int64_t &size, int &headerLength) -> bool
{
    const int first = r.U8(position);
    if (first <= 0)
    {
        return false;
    }
    int idLength = 1;
    while ((first & (0x80 >> (idLength - 1))) == 0 && idLength < 5)
    {
        ++idLength;
    }
    if (idLength > 4)
    {
        return false;
    }
    id = 0;
    for (int i = 0; i < idLength; ++i)
    {
        const int b = r.U8(position + i);
        if (b < 0)
        {
            return false;
        }
        id = (id << 8) | b;
    }
    const int sizeFirst = r.U8(position + idLength);
    if (sizeFirst <= 0)
    {
        return false;
    }
    int sizeLength = 1;
    while ((sizeFirst & (0x80 >> (sizeLength - 1))) == 0)
    {
        ++sizeLength;
    }
    size = sizeFirst & (0xFF >> sizeLength);
    bool allOnes = size == (0xFF >> sizeLength);
    for (int i = 1; i < sizeLength; ++i)
    {
        const int b = r.U8(position + idLength + i);
        if (b < 0)
        {
            return false;
        }
        size = (size << 8) | b;
        allOnes = allOnes && b == 0xFF;
    }
    headerLength = idLength + sizeLength;
    return !allOnes;
}

auto FindMatroska(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    std::int64_t id = 0, size = 0;
    int header = 0;
    if (!EbmlElement(r, start, id, size, header) || id != 0x1A45DFA3 || size > 1024)
    {
        return std::nullopt;
    }
    // the document type is in the header
    std::string docType;
    std::int64_t position = start + header;
    while (position < start + header + size)
    {
        std::int64_t childId = 0, childSize = 0;
        int childHeader = 0;
        if (!EbmlElement(r, position, childId, childSize, childHeader) || childSize > 64)
        {
            return std::nullopt;
        }
        if (childId == 0x4282)
        {
            docType.resize(static_cast<std::size_t>(childSize));
            if (childSize > 0 && !r.Get(position + childHeader, docType.data(), docType.size()))
            {
                return std::nullopt;
            }
        }
        position += childHeader + childSize;
    }
    while (!docType.empty() && docType.back() == '\0')
    {
        docType.pop_back();
    }
    if (docType != "matroska" && docType != "webm")
    {
        return std::nullopt;
    }
    std::int64_t segmentId = 0, segmentSize = 0;
    int segmentHeader = 0;
    if (!EbmlElement(r, start + header + size, segmentId, segmentSize, segmentHeader) || segmentId != 0x18538067)
    {
        return std::nullopt;
    }
    return Found{header + size + segmentHeader + segmentSize, docType == "webm" ? "webm" : "mkv"};
}

auto FindEmf(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const auto size = r.U32Le(start + 48);
    if (r.U32Le(start) != 1 || r.U32Le(start + 4) < 88 || size < 88 || start + size > r.Size() || r.U32Le(start + 44) != 0x10000)
    {
        return std::nullopt;
    }
    return Found{size, "emf"};
}

auto FindWmf(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    const auto type = r.U16Le(start + 22);
    const auto headerSize = r.U16Le(start + 24);
    const auto words = r.U32Le(start + 28);
    if (r.U16Le(start + 4) != 0 || r.U32Le(start + 16) != 0 || (type != 0 && type != 1) || headerSize != 9 || r.U16Le(start + 38) != 0 || words < 9)
    {
        return std::nullopt;
    }
    return Found{22 + words * 2, "wmf"};
}

auto FindLbm(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    if (!r.Matches(start + 8, "ILBM") || !r.Matches(start + 12, "BMHD"))
    {
        return std::nullopt;
    }
    const auto size = r.U32Be(start + 4) + 8;
    if (size < 40)
    {
        return std::nullopt;
    }
    return Found{size, "lbm"};
}

auto FindTga(CachedReader &r, std::int64_t start, const HyperRipper::Options &) -> std::optional<Found>
{
    std::array<std::uint8_t, 18> h{};
    if (!r.Get(start, h.data(), h.size()))
    {
        return std::nullopt;
    }
    const int bits = h[16];
    const int width = h[12] | (h[13] << 8);
    const int height = h[14] | (h[15] << 8);
    const bool origin = h[8] == 0 && h[9] == 0 && h[10] == 0 && h[11] == 0;
    const bool mapped = h[1] == 1 && h[2] == 1; // uncompressed with a palette
    const bool truecolor = h[1] == 0 && h[2] == 2;
    if ((!mapped && !truecolor) || !origin || width < 2 || height < 2 || width > 2560 || height > 2048 || (h[17] & 0xC0) != 0)
    {
        return std::nullopt;
    }
    std::int64_t paletteBytes = 0;
    if (mapped)
    {
        const int entryBits = h[7];
        const int entries = h[5] | (h[6] << 8);
        if (bits != 8 || (entryBits != 15 && entryBits != 16 && entryBits != 24 && entryBits != 32) || entries < 1 || entries > 256 || (h[3] | (h[4] << 8)) + entries > 256)
        {
            return std::nullopt;
        }
        paletteBytes = entries * ((entryBits + 7) / 8);
    }
    else if (h[3] != 0 || h[4] != 0 || h[5] != 0 || h[6] != 0 || h[7] != 0 || (bits != 8 && bits != 16 && bits != 24 && bits != 32))
    {
        return std::nullopt;
    }
    std::int64_t size = 18 + h[0] + paletteBytes + static_cast<std::int64_t>(width) * height * bits / 8;
    if (start + size > r.Size())
    {
        return std::nullopt;
    }
    if (r.Matches(start + size + 8, "TRUEVISION-XFILE") && start + size + 26 <= r.Size())
    {
        size += 26;
    }
    return Found{size, "tga"};
}

const std::vector<Signature> &AllSignatures()
{
    static const std::vector<Signature> signatures{
        {"PNG", "png", "PNG image", std::string("\x89PNG\r\n\x1a\n", 8), &FindPng},
        {"JPEG", "jpg", "JPEG image", std::string("\xff\xd8\xff", 3), &FindJpeg},
        {"GIF", "gif", "GIF image", "GIF8", &FindGif},
        {"BMP", "bmp", "Windows bitmap", "BM", &FindBmp},
        {"RIFF", "wav", "RIFF: WAV sound, AVI video, WebP image", "RIFF", &FindRiff},
        {"OGG", "ogg", "Ogg stream (Vorbis, Opus...)", "OggS", &FindOgg},
        {"ZIP", "zip", "ZIP archive", std::string("PK\x03\x04", 4), &FindZip},
        {"DDS", "dds", "DirectDraw Surface texture", "DDS ", &FindDds},
        {"MIDI", "mid", "Standard MIDI file", "MThd", &FindMidi},
        {"VOC", "voc", "Creative Voice file", "Creative Voice File", &FindVoc},
        {"669", "669", "669 tracker module", "if", &FindModule669},
        {"669 EXT", "669", "Extended 669 tracker module", "JN", &FindModule669},
        {"XM", "xm", "FastTracker 2 module", "Extended Module: ", &FindXm},
        {"S3M", "s3m", "Scream Tracker 3 module", "SCRM", &FindS3m, 44},
        {"IT", "it", "Impulse Tracker module", "IMPM", &FindIt},
        {"MP3", "mp3", "MPEG audio (layers I to III)", std::string("\xff", 1), &FindMp3},
        {"MP3", "mp3", "MPEG audio with an ID3v2 tag", "ID3", &FindMp3},
        {"AAC", "aac", "AAC audio (ADTS)", std::string("\xff", 1), &FindAac},
        {"MP4", "mp4", "MPEG-4 and QuickTime: video, M4A audio", "ftyp", &FindIsoMedia, 4},
        {"MOV", "mov", "QuickTime movie without ftyp", "moov", &FindIsoMedia, 4},
        {"BIK", "bik", "Bink video", "BIK", &FindBink},
        {"FLI", "fli", "Autodesk Animator FLIC", std::string("\x11\xaf", 2), &FindFlic, 4},
        {"FLC", "flc", "Autodesk Animator Pro FLC", std::string("\x12\xaf", 2), &FindFlic, 4},
        {"MKV", "mkv", "Matroska and WebM video", std::string("\x1a\x45\xdf\xa3", 4), &FindMatroska},
        {"EMF", "emf", "Windows enhanced metafile", " EMF", &FindEmf, 40},
        {"WMF", "wmf", "Windows metafile (placeable)", std::string("\xd7\xcd\xc6\x9a", 4), &FindWmf},
        {"LBM", "lbm", "Interleaved bitmap (EA IFF)", "FORM", &FindLbm},
        {"TGA", "tga", "Targa image (uncompressed)", std::string("\0\0\x02", 3), &FindTga},
        {"TGA", "tga", "Targa image (uncompressed, palette)", std::string("\0\x01\x01", 3), &FindTga},
    };
    return signatures;
}

class FileReader : public RipReader
{
  public:
    explicit FileReader(const std::filesystem::path &path) : _stream(path, std::ios::binary)
    {
        if (!_stream)
        {
            throw std::runtime_error("cannot open " + path.string());
        }
        _stream.seekg(0, std::ios::end);
        _size = static_cast<std::int64_t>(_stream.tellg());
    }
    [[nodiscard]] auto Size() const -> std::int64_t override
    {
        return _size;
    }
    auto Read(std::int64_t offset, void *buffer, std::size_t length) const -> std::size_t override
    {
        const std::scoped_lock lock(_mutex);
        _stream.clear();
        _stream.seekg(offset);
        _stream.read(static_cast<char *>(buffer), static_cast<std::streamsize>(length));
        return static_cast<std::size_t>(_stream.gcount());
    }

  private:
    mutable std::ifstream _stream;
    mutable std::mutex _mutex;
    std::int64_t _size{0};
};

/// The archive of the files found.
class RippedArchive : public Dudi7::IArchive
{
  public:
    RippedArchive(std::filesystem::path file, std::shared_ptr<const std::vector<RippedFile>> files, std::shared_ptr<const std::vector<std::string>> names)
        : _file(std::move(file)), _files(std::move(files)), _names(std::move(names))
    {
    }
    [[nodiscard]] auto GetEntryCount() const -> std::int64_t override
    {
        return static_cast<std::int64_t>(_files->size());
    }
    [[nodiscard]] auto GetFormatId() const -> const std::string & override
    {
        static const std::string id = "HyperRipper";
        return id;
    }
    [[nodiscard]] auto GetDirSeparator() const -> const std::string & override
    {
        static const std::string separator = "\\";
        return separator;
    }
    [[nodiscard]] auto GetEntry(std::int64_t index) const -> Dudi7::EntryInfo override
    {
        if (index < 0 || index >= GetEntryCount())
        {
            throw Dudi7::Error(DUDI7_ERR_BADARG, "invalid entry number");
        }
        const auto &file = (*_files)[static_cast<std::size_t>(index)];
        return Dudi7::EntryInfo{(*_names)[static_cast<std::size_t>(index)], file.Offset, file.Size};
    }
    auto Extract(std::int64_t index, const std::function<bool(std::span<const std::byte>)> &sink) const -> int override
    {
        if (index < 0 || index >= GetEntryCount())
        {
            return DUDI7_ERR_BADARG;
        }
        const auto &file = (*_files)[static_cast<std::size_t>(index)];
        std::ifstream stream(_file, std::ios::binary);
        if (!stream)
        {
            return DUDI7_ERR_IO;
        }
        stream.seekg(file.Offset);
        std::vector<std::byte> buffer(64 * 1024);
        std::int64_t left = file.Size;
        while (left > 0)
        {
            const auto want = static_cast<std::streamsize>(std::min<std::int64_t>(left, static_cast<std::int64_t>(buffer.size())));
            stream.read(reinterpret_cast<char *>(buffer.data()), want);
            const auto got = stream.gcount();
            if (got <= 0)
            {
                return DUDI7_ERR_IO;
            }
            if (!sink(std::span<const std::byte>(buffer.data(), static_cast<std::size_t>(got))))
            {
                return DUDI7_ERR_CANCELLED;
            }
            left -= got;
        }
        return DUDI7_OK;
    }

  private:
    std::filesystem::path _file;
    std::shared_ptr<const std::vector<RippedFile>> _files;
    std::shared_ptr<const std::vector<std::string>> _names;
};
} // namespace

auto HyperRipper::Signatures() -> std::vector<SignatureInfo>
{
    static const std::set<std::string> noisy{"669", "669 EXT", "TGA", "AAC"};
    std::vector<SignatureInfo> result;
    for (const auto &signature : AllSignatures())
    {
        if (std::none_of(result.begin(), result.end(), [&](const SignatureInfo &known) { return known.Type == signature.Type; }))
        {
            unsigned kinds = 0;
            if (std::string_view(signature.Type) == "RIFF")
            {
                kinds = RIPPER_AUDIO | RIPPER_VIDEO | RIPPER_IMAGE;
            }
            else
            {
                switch (GetRipKind(signature.Extension))
                {
                case RipKind::Audio:
                    kinds = RIPPER_AUDIO;
                    break;
                case RipKind::Video:
                    kinds = RIPPER_VIDEO;
                    break;
                case RipKind::Image:
                    kinds = RIPPER_IMAGE;
                    break;
                case RipKind::Unknown:
                    break;
                }
            }
            result.push_back({signature.Type, signature.Extension, signature.Description, kinds, noisy.contains(signature.Type)});
        }
    }
    return result;
}

auto HyperRipper::Scan(const RipReader &reader, const Options &options, const std::function<void(std::int64_t, std::int64_t)> &progress,
                       const std::stop_token &stop) -> std::vector<RippedFile>
{
    // candidate signatures by first byte
    std::array<std::vector<const Signature *>, 256> byFirstByte;
    for (const auto &signature : AllSignatures())
    {
        if (options.Types.empty() || options.Types.contains(signature.Type))
        {
            byFirstByte[static_cast<std::uint8_t>(signature.Magic[0])].push_back(&signature);
        }
    }
    CachedReader cached(reader);
    std::vector<RippedFile> found;
    const std::int64_t total = reader.Size();
    std::vector<std::uint8_t> block(std::clamp(options.BlockSize, MIN_BLOCK, MAX_BLOCK) + MAX_MAGIC);
    std::int64_t position = 0;
    std::int64_t reportedAt = 0;
    std::int64_t notBefore = 0; // (a file whose magic number is inside of it must not start in the previous find)
    while (position < total)
    {
        if (stop.stop_requested())
        {
            break;
        }
        const std::size_t got = reader.Read(position, block.data(), block.size());
        if (got == 0)
        {
            break;
        }
        const std::size_t limit = got > MAX_MAGIC ? (position + static_cast<std::int64_t>(got) >= total ? got : got - MAX_MAGIC) : got;
        std::size_t i = 0;
        std::int64_t jumpTo = -1;
        for (; i < limit; ++i)
        {
            const auto &candidates = byFirstByte[block[i]];
            if (candidates.empty())
            {
                continue;
            }
            const std::int64_t at = position + static_cast<std::int64_t>(i);
            for (const Signature *signature : candidates)
            {
                const std::size_t magic = signature->Magic.size();
                if (i + magic > got || std::memcmp(&block[i], signature->Magic.data(), magic) != 0)
                {
                    continue;
                }
                const std::int64_t fileStart = at - signature->MagicBack;
                if (fileStart < 0 || fileStart < notBefore)
                {
                    continue;
                }
                auto result = signature->Find(cached, fileStart, options);
                if (result && result->Size >= options.MinSize && fileStart + result->Size <= total)
                {
                    found.push_back({fileStart, result->Size, signature->Type, result->Extension});
                    if (options.SkipInsideFound)
                    {
                        jumpTo = fileStart + result->Size;
                        notBefore = jumpTo;
                    }
                    break;
                }
            }
            if (jumpTo >= 0)
            {
                break;
            }
        }
        position = jumpTo >= 0 ? jumpTo : position + static_cast<std::int64_t>(i);
        if (progress && position - reportedAt >= 1024 * 1024)
        {
            progress(position, total);
            reportedAt = position;
        }
    }
    if (progress)
    {
        progress(total, total);
    }
    return found;
}

auto HyperRipper::Scan(const std::filesystem::path &file) -> std::vector<RippedFile>
{
    return Scan(file, Options{});
}

auto HyperRipper::Scan(const std::filesystem::path &file, const Options &options) -> std::vector<RippedFile>
{
    const FileReader reader(file);
    return Scan(reader, options, {}, std::stop_token{});
}

RippedArchiveSource::RippedArchiveSource(std::filesystem::path file, std::vector<RippedFile> files)
    : _file(std::move(file)), _files(std::make_shared<const std::vector<RippedFile>>(std::move(files)))
{
    _names = std::make_shared<const std::vector<std::string>>(MakeRipNames(_file, *_files, NamingOptions{}));
}

auto RippedArchiveSource::Open() const -> std::unique_ptr<Dudi7::IArchive>
{
    return std::make_unique<RippedArchive>(_file, _files, _names);
}

HyperRipperJob::HyperRipperJob(std::filesystem::path file, HyperRipper::Options options) : _file(std::move(file)), _options(std::move(options))
{
}

HyperRipperJob::~HyperRipperJob()
{
    Cancel();
}

void HyperRipperJob::Start()
{
    _thread = std::jthread([this](const std::stop_token &stop) {
        try
        {
            const FileReader reader(_file);
            _bytesTotal = reader.Size();
            auto results = HyperRipper::Scan(reader, _options, [this](std::int64_t done, std::int64_t) { _bytesDone = done; }, stop);
            _cancelled = stop.stop_requested();
            const std::scoped_lock lock(_mutex);
            _results = std::move(results);
        }
        catch (const std::exception &error)
        {
            const std::scoped_lock lock(_mutex);
            _error = error.what();
        }
        _done = true;
    });
}

void HyperRipperJob::Cancel()
{
    _thread.request_stop();
}

auto HyperRipperJob::GetResults() const -> std::vector<RippedFile>
{
    const std::scoped_lock lock(_mutex);
    return _results;
}

auto HyperRipperJob::GetError() const -> std::string
{
    const std::scoped_lock lock(_mutex);
    return _error;
}
} // namespace Dup::Core
