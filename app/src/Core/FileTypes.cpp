// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/FileTypes.h"

#include "Core/ListTemplate.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>

namespace Dup::Core
{
namespace
{
using Cat = FileCategory;

/// The first bytes of a file with the reads that the checks need (all of them stay inside the buffer).
class Head
{
  public:
    Head(std::span<const std::byte> data, std::int64_t size) : _data(data), _size(size)
    {
    }
    [[nodiscard]] auto Size() const -> std::size_t
    {
        return _data.size();
    }
    [[nodiscard]] auto FileSize() const -> std::int64_t
    {
        return _size;
    }
    [[nodiscard]] auto Has(std::size_t offset, std::size_t length) const -> bool
    {
        return offset <= _data.size() && length <= _data.size() - offset;
    }
    [[nodiscard]] auto U8(std::size_t at) const -> int
    {
        return at < _data.size() ? static_cast<std::uint8_t>(_data[at]) : -1;
    }
    [[nodiscard]] auto U16(std::size_t at) const -> std::int64_t
    {
        return Has(at, 2) ? (U8(at) | (U8(at + 1) << 8)) : -1;
    }
    [[nodiscard]] auto U16Be(std::size_t at) const -> std::int64_t
    {
        return Has(at, 2) ? ((U8(at) << 8) | U8(at + 1)) : -1;
    }
    [[nodiscard]] auto U32(std::size_t at) const -> std::int64_t
    {
        return Has(at, 4) ? (static_cast<std::int64_t>(U8(at)) | (U8(at + 1) << 8) | (U8(at + 2) << 16) | (static_cast<std::int64_t>(U8(at + 3)) << 24)) : -1;
    }
    [[nodiscard]] auto U32Be(std::size_t at) const -> std::int64_t
    {
        return Has(at, 4) ? ((static_cast<std::int64_t>(U8(at)) << 24) | (U8(at + 1) << 16) | (U8(at + 2) << 8) | U8(at + 3)) : -1;
    }
    [[nodiscard]] auto Is(std::size_t at, std::string_view text) const -> bool
    {
        return Has(at, text.size()) && std::memcmp(_data.data() + at, text.data(), text.size()) == 0;
    }
    [[nodiscard]] auto Text(std::size_t at, std::size_t length) const -> std::string
    {
        return Has(at, length) ? std::string(reinterpret_cast<const char *>(_data.data()) + at, length) : std::string();
    }
    [[nodiscard]] auto Bytes() const -> std::span<const std::byte>
    {
        return _data;
    }

  private:
    std::span<const std::byte> _data;
    std::int64_t _size;
};

auto Make(const char *id, const char *description, const char *extension, Cat category, int confidence) -> DetectedType
{
    return {id, description, extension, category, confidence};
}

using Validator = int (*)(const Head &); // 0 = not this type, else the confidence

struct Signature
{
    const char *Id;
    const char *Description;
    const char *Extension;
    Cat Category;
    std::size_t Offset;
    std::string_view Magic;
    int Confidence;
    Validator Validate;
};

// ---- validators ----------------------------------------------------------------------------------------------------

auto IsBmp(const Head &h) -> int
{
    const auto dib = h.U32(14);
    const bool known = dib == 12 || dib == 40 || dib == 52 || dib == 56 || dib == 64 || dib == 108 || dib == 124;
    return known && h.U32(6) == 0 ? 90 : 0;
}

auto IsIconLike(const Head &h) -> int
{
    const auto count = h.U16(4);
    if (count < 1 || count > 64 || !h.Has(6, static_cast<std::size_t>(count) * 16))
    {
        return 0;
    }
    for (std::size_t i = 0; i < static_cast<std::size_t>(count); ++i)
    {
        const std::size_t at = 6 + i * 16;
        const auto bits = h.U16(at + 6), planes = h.U16(at + 4);
        const auto size = h.U32(at + 8), offset = h.U32(at + 12);
        const bool cursor = h.U8(2) == 2; // the hot spot of a cursor replaces the planes and the bits
        if (h.U8(at + 3) != 0 || (!cursor && (planes > 1 || (bits != 0 && bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32))) || size <= 0 ||
            offset < 6 + count * 16 || offset + size > h.FileSize())
        {
            return 0;
        }
    }
    return 85;
}

auto IsPcx(const Head &h) -> int
{
    const int version = h.U8(1);
    const int bits = h.U8(3);
    return (version == 0 || version == 2 || version == 3 || version == 4 || version == 5) && h.U8(2) == 1 && (bits == 1 || bits == 2 || bits == 4 || bits == 8) &&
                   h.U16(8) >= h.U16(4) && h.U16(10) >= h.U16(6)
               ? 70
               : 0;
}

auto IsFlic(const Head &h) -> int
{
    const auto magic = h.U16(4);
    return (magic == 0xAF11 || magic == 0xAF12) && h.U32(0) <= h.FileSize() && h.U16(6) >= 1 && h.U16(6) <= 4000 && h.U16(8) >= 1 && h.U16(8) <= 4096 && h.U16(12) == 8 ? 80 : 0;
}

auto IsTiff(const Head &h) -> int
{
    return (h.Is(0, std::string_view("II*\0", 4)) || h.Is(0, std::string_view("MM\0*", 4))) ? 90 : 0;
}

auto IsPackFile(const Head &h) -> int
{
    const auto offset = h.U32(4), length = h.U32(8);
    return offset >= 12 && offset <= h.FileSize() && length >= 0 && length % 64 == 0 && offset + length <= h.FileSize() ? 90 : 0;
}

auto IsDoomWad(const Head &h) -> int
{
    const auto lumps = h.U32(4), directory = h.U32(8);
    return lumps >= 0 && lumps < 1000000 && directory >= 12 && directory <= h.FileSize() ? 95 : 0;
}

auto IsBig(const Head &h) -> int
{
    return h.U32(4) == h.FileSize() || h.U32Be(4) == h.FileSize() ? 95 : 60;
}

auto IsVp(const Head &h) -> int
{
    return h.U32(4) >= 1 && h.U32(4) <= 3 ? 95 : 0;
}

// A picture of Doom: width, height, offsets, then one offset per column; a column is posts (row, length, a byte, the pixels, a byte)
// ended by 0xFF. The columns that are inside of the head are walked: the structure is what tells it from other data.
auto IsDoomPicture(const Head &h) -> int
{
    const auto width = h.U16(0), height = h.U16(2);
    if (width < 1 || width > 2048 || height < 1 || height > 2048 || std::abs(static_cast<std::int16_t>(h.U16(4))) > 2048 || !h.Has(8, static_cast<std::size_t>(width) * 4))
    {
        return 0;
    }
    const std::int64_t first = 8 + width * 4;
    const std::int64_t limit = std::min<std::int64_t>(h.FileSize(), 1LL << 31);
    std::int64_t previous = first - 1;
    int walked = 0;
    for (std::int64_t column = 0; column < width; ++column)
    {
        const auto offset = h.U32(8 + static_cast<std::size_t>(column) * 4);
        if (offset < first || offset >= limit || offset <= previous)
        {
            return 0;
        }
        previous = offset;
        if (static_cast<std::size_t>(offset) + 2 > h.Size())
        {
            continue; // further than what was read
        }
        std::size_t at = static_cast<std::size_t>(offset);
        bool ended = false;
        for (int posts = 0; posts < 260; ++posts)
        {
            const int top = h.U8(at);
            if (top < 0)
            {
                ended = true; // runs out of the head: not checked further
                break;
            }
            if (top == 0xFF)
            {
                ended = true;
                break;
            }
            const int length = h.U8(at + 1);
            if (length < 0)
            {
                ended = true;
                break;
            }
            if (top + length > height + 255 || length == 0)
            {
                return 0;
            }
            at += static_cast<std::size_t>(length) + 4;
        }
        if (!ended)
        {
            return 0;
        }
        ++walked;
    }
    return walked > 0 ? 85 : 0;
}

// A sound of Doom (DMX): format 3, the rate, the number of samples that fills the file
auto IsDmxSound(const Head &h) -> int
{
    const auto rate = h.U16(2), samples = h.U32(4);
    return h.U16(0) == 3 && rate >= 4000 && rate <= 48000 && samples >= 32 && samples + 8 <= h.FileSize() && samples + 8 >= h.FileSize() - 32 ? 80 : 0;
}

// The level of Quake (version 29) and Half-Life (30): the version, then the offset and the length of each lump
auto IsQuakeBsp(const Head &h) -> int
{
    const auto version = h.U32(0);
    if (version != 29 && version != 30)
    {
        return 0;
    }
    for (std::size_t lump = 0; lump < 15; ++lump)
    {
        const auto offset = h.U32(4 + lump * 8), length = h.U32(8 + lump * 8);
        if (offset < 0 || length < 0 || offset + length > h.FileSize() + 16)
        {
            return 0;
        }
    }
    return 90;
}

auto IsMod(const Head &h) -> int
{
    static const char *const tags[] = {"M.K.", "M!K!", "4CHN", "6CHN", "8CHN", "FLT4", "FLT8", "CD81", "OCTA", "OKTA", "16CH", "32CH"};
    if (!h.Has(1080, 4))
    {
        return 0;
    }
    for (const char *tag : tags)
    {
        if (h.Is(1080, tag))
        {
            return 85;
        }
    }
    return 0;
}

auto IsS3m(const Head &h) -> int
{
    return h.Is(44, "SCRM") && h.U8(28) == 0x1A ? 90 : 0;
}

// MPEG audio: a frame header that makes sense, followed by another one when the head is long enough
auto MpegFrameLength(const Head &h, std::size_t at) -> std::int64_t
{
    const auto header = h.U32Be(at);
    if (header < 0 || ((header >> 21) & 0x7FF) != 0x7FF)
    {
        return 0;
    }
    const int version = static_cast<int>((header >> 19) & 3), layer = static_cast<int>((header >> 17) & 3);
    const int bitrate = static_cast<int>((header >> 12) & 15), rate = static_cast<int>((header >> 10) & 3);
    if (version == 1 || layer == 0 || bitrate == 0 || bitrate == 15 || rate == 3)
    {
        return 0;
    }
    static constexpr int V1L3[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static constexpr int V1L2[16] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0};
    static constexpr int V1L1[16] = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0};
    static constexpr int V2L3[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    static constexpr int V2L1[16] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0};
    static constexpr int RATES[3] = {44100, 48000, 32000};
    int hz = RATES[rate];
    if (version == 2)
    {
        hz /= 2;
    }
    else if (version == 0)
    {
        hz /= 4;
    }
    const int *table = version == 3 ? (layer == 1 ? V1L3 : layer == 2 ? V1L2 : V1L1) : (layer == 3 ? V2L1 : V2L3);
    const std::int64_t bits = table[bitrate] * 1000LL;
    const std::int64_t padding = (header >> 9) & 1;
    if (layer == 3)
    {
        return (12 * bits / hz + padding) * 4;
    }
    return (layer == 1 && version != 3 ? 72 : 144) * bits / hz + padding;
}

auto IsMpegAudio(const Head &h) -> int
{
    const auto length = MpegFrameLength(h, 0);
    if (length < 24)
    {
        return 0;
    }
    if (static_cast<std::int64_t>(h.Size()) >= length + 4)
    {
        return MpegFrameLength(h, static_cast<std::size_t>(length)) >= 24 ? 85 : 0;
    }
    return 50;
}

auto IsId3(const Head &h) -> int
{
    return h.U8(3) < 0xFF && h.U8(4) < 0xFF && h.U8(6) < 0x80 && h.U8(7) < 0x80 && h.U8(8) < 0x80 && h.U8(9) < 0x80 ? 90 : 0;
}

auto IsAdts(const Head &h) -> int
{
    return h.U8(0) == 0xFF && (h.U8(1) & 0xF6) == 0xF0 && ((h.U8(2) >> 2) & 15) < 13 && h.Has(0, 7) && (((h.U8(3) & 3) << 11) | (h.U8(4) << 3) | (h.U8(5) >> 5)) >= 7 ? 70 : 0;
}

auto IsAdx(const Head &h) -> int
{
    const auto copyright = h.U16Be(2);
    return h.U8(0) == 0x80 && h.U8(1) == 0 && copyright >= 6 && h.Is(static_cast<std::size_t>(copyright) - 2, "(c)CRI") ? 100 : 0;
}

auto IsMpegPs(const Head &h) -> int
{
    return h.U8(4) >> 6 == 1 || (h.U8(4) >> 4) == 2 ? 80 : 0; // MPEG-2 or MPEG-1 pack header
}

auto IsBink(const Head &h) -> int
{
    const int revision = h.U8(3);
    return (revision == 'b' || revision == 'f' || revision == 'g' || revision == 'h' || revision == 'i' || revision == 'k') && h.U32(4) + 8 <= h.FileSize() + 8 ? 90 : 0;
}

auto IsTtf(const Head &h) -> int
{
    const auto tables = h.U16Be(4);
    return tables >= 4 && tables <= 40 ? 85 : 0;
}

auto IsPe(const Head &h) -> int
{
    const auto pe = h.U32(0x3C);
    return pe >= 0x40 && pe + 24 <= static_cast<std::int64_t>(h.Size()) && h.Is(static_cast<std::size_t>(pe), std::string_view("PE\0\0", 4)) ? 95 : 55;
}

auto IsJavaOrMachO(const Head &h) -> int
{
    const auto major = h.U16Be(6);
    return major >= 45 && major <= 75 ? 90 : 0;
}

auto IsGzip(const Head &h) -> int
{
    return h.U8(2) == 8 && (h.U8(3) & 0xE0) == 0 ? 95 : 0;
}

auto IsBz2(const Head &h) -> int
{
    return h.U8(3) >= '1' && h.U8(3) <= '9' && h.Is(4, "1AY&SY") ? 100 : 0;
}

auto IsLzh(const Head &h) -> int
{
    return (h.Is(2, "-lh") || h.Is(2, "-lz")) && h.U8(6) == '-' ? 90 : 0;
}

auto IsArj(const Head &h) -> int
{
    return h.U16(2) > 0 && h.U16(2) < 2600 ? 70 : 0;
}

auto IsZlib(const Head &h) -> int
{
    const int cmf = h.U8(0), flg = h.U8(1);
    return (cmf & 0x0F) == 8 && (cmf >> 4) <= 7 && ((cmf << 8) | flg) % 31 == 0 && (flg & 0x20) == 0 ? 45 : 0;
}

auto IsRoq(const Head &h) -> int
{
    return h.U16(6) >= 1 && h.U16(6) <= 60 ? 100 : 60;
}

auto IsXnb(const Head &h) -> int
{
    const int platform = h.U8(3);
    return (platform == 'w' || platform == 'm' || platform == 'x' || platform == 'i' || platform == 'a' || platform == 'p' || platform == 'n') && h.U8(4) >= 3 && h.U8(4) <= 5 ? 95 : 0;
}

auto IsMpq(const Head &h) -> int
{
    return h.U8(3) == 0x1A || h.U8(3) == 0x1B ? 100 : 0;
}

// the order matters: a longer or more specific signature before a shorter one
const std::vector<Signature> &Table()
{
    static const std::vector<Signature> table{
        // images
        {"png", "PNG image", "png", Cat::IMAGE, 0, std::string_view("\x89PNG\r\n\x1a\n", 8), 100, nullptr},
        {"jpg", "JPEG image", "jpg", Cat::IMAGE, 0, std::string_view("\xff\xd8\xff", 3), 95, nullptr},
        {"gif", "GIF image", "gif", Cat::IMAGE, 0, "GIF87a", 100, nullptr},
        {"gif", "GIF image", "gif", Cat::IMAGE, 0, "GIF89a", 100, nullptr},
        {"bmp", "Windows bitmap", "bmp", Cat::IMAGE, 0, "BM", 90, &IsBmp},
        {"tiff", "TIFF image", "tif", Cat::IMAGE, 0, std::string_view("II*\0", 4), 90, &IsTiff},
        {"tiff", "TIFF image", "tif", Cat::IMAGE, 0, std::string_view("MM\0*", 4), 90, &IsTiff},
        {"dds", "DirectDraw Surface texture", "dds", Cat::IMAGE, 0, "DDS ", 100, nullptr},
        {"ktx", "Khronos KTX texture", "ktx", Cat::IMAGE, 0, std::string_view("\xabKTX 11\xbb\r\n\x1a\n", 12), 100, nullptr},
        {"ktx2", "Khronos KTX 2 texture", "ktx2", Cat::IMAGE, 0, std::string_view("\xabKTX 20\xbb\r\n\x1a\n", 12), 100, nullptr},
        {"pvr", "PowerVR texture", "pvr", Cat::IMAGE, 0, std::string_view("PVR\x03", 4), 90, nullptr},
        {"psd", "Photoshop image", "psd", Cat::IMAGE, 0, "8BPS", 95, nullptr},
        {"exr", "OpenEXR image", "exr", Cat::IMAGE, 0, std::string_view("\x76\x2f\x31\x01", 4), 100, nullptr},
        {"hdr", "Radiance HDR image", "hdr", Cat::IMAGE, 0, "#?RADIANCE", 100, nullptr},
        {"qoi", "QOI image", "qoi", Cat::IMAGE, 0, "qoif", 100, nullptr},
        {"jp2", "JPEG 2000 image", "jp2", Cat::IMAGE, 0, std::string_view("\0\0\0\x0cjP  \r\n\x87\n", 12), 100, nullptr},
        {"sti", "Sir-Tech STI image", "sti", Cat::IMAGE, 0, "STCI", 100, nullptr},
        {"fli", "Autodesk Animator animation", "flc", Cat::VIDEO, 4, std::string_view("\x11\xaf", 2), 80, &IsFlic},
        {"fli", "Autodesk Animator animation", "flc", Cat::VIDEO, 4, std::string_view("\x12\xaf", 2), 80, &IsFlic},
        {"ico", "Windows icon", "ico", Cat::IMAGE, 0, std::string_view("\0\0\1\0", 4), 70, &IsIconLike},
        {"cur", "Windows cursor", "cur", Cat::IMAGE, 0, std::string_view("\0\0\2\0", 4), 70, &IsIconLike},
        {"pcx", "PCX image", "pcx", Cat::IMAGE, 0, std::string_view("\x0a", 1), 70, &IsPcx},
        // audio
        {"flac", "FLAC audio", "flac", Cat::AUDIO, 0, "fLaC", 100, nullptr},
        {"mus", "Doom MUS music", "mus", Cat::AUDIO, 0, "MUS\x1a", 100, nullptr},
        {"midi", "MIDI music", "mid", Cat::AUDIO, 0, std::string_view("MThd\0\0\0\6", 8), 100, nullptr},
        {"xm", "FastTracker 2 module", "xm", Cat::AUDIO, 0, "Extended Module: ", 100, nullptr},
        {"it", "Impulse Tracker module", "it", Cat::AUDIO, 0, "IMPM", 90, nullptr},
        {"s3m", "Scream Tracker 3 module", "s3m", Cat::AUDIO, 44, "SCRM", 90, &IsS3m},
        {"voc", "Creative Voice audio", "voc", Cat::AUDIO, 0, "Creative Voice File\x1a", 100, nullptr},
        {"au", "Sun/NeXT audio", "au", Cat::AUDIO, 0, ".snd", 90, nullptr},
        {"fsb", "FMOD sound bank (FSB5)", "fsb", Cat::AUDIO, 0, "FSB5", 100, nullptr},
        {"fsb", "FMOD sound bank (FSB4)", "fsb", Cat::AUDIO, 0, "FSB4", 100, nullptr},
        {"fsb", "FMOD sound bank (FSB3)", "fsb", Cat::AUDIO, 0, "FSB3", 100, nullptr},
        {"bnk", "Wwise sound bank", "bnk", Cat::AUDIO, 0, "BKHD", 100, nullptr},
        {"xwb", "XACT wave bank", "xwb", Cat::AUDIO, 0, "WBND", 100, nullptr},
        {"xsb", "XACT sound bank", "xsb", Cat::AUDIO, 0, "SDBK", 100, nullptr},
        {"adx", "CRI ADX audio", "adx", Cat::AUDIO, 0, std::string_view("\x80\x00", 2), 100, &IsAdx},
        {"mp3", "MP3 audio (ID3 tag)", "mp3", Cat::AUDIO, 0, "ID3", 90, &IsId3},
        {"wma", "Windows Media (ASF)", "wma", Cat::VIDEO, 0, std::string_view("\x30\x26\xb2\x75\x8e\x66\xcf\x11", 8), 100, nullptr},
        // video
        {"mkv", "Matroska / WebM video", "mkv", Cat::VIDEO, 0, std::string_view("\x1a\x45\xdf\xa3", 4), 100, nullptr},
        {"bik", "Bink video", "bik", Cat::VIDEO, 0, "BIK", 90, &IsBink},
        {"smk", "Smacker video", "smk", Cat::VIDEO, 0, "SMK2", 100, nullptr},
        {"smk", "Smacker video", "smk", Cat::VIDEO, 0, "SMK4", 100, nullptr},
        {"flv", "Flash video", "flv", Cat::VIDEO, 0, std::string_view("FLV\x01", 4), 100, nullptr},
        {"roq", "Id RoQ video", "roq", Cat::VIDEO, 0, std::string_view("\x84\x10\xff\xff\xff\xff", 6), 100, &IsRoq},
        {"ivf", "VP8/VP9 IVF video", "ivf", Cat::VIDEO, 0, "DKIF", 100, nullptr},
        {"thp", "Nintendo THP video", "thp", Cat::VIDEO, 0, std::string_view("THP\0", 4), 100, nullptr},
        {"mpg", "MPEG program stream", "mpg", Cat::VIDEO, 0, std::string_view("\0\0\1\xba", 4), 80, &IsMpegPs},
        {"mpg", "MPEG video", "mpg", Cat::VIDEO, 0, std::string_view("\0\0\1\xb3", 4), 80, nullptr},
        // archives and compression
        {"rar", "RAR archive (version 5)", "rar", Cat::ARCHIVE, 0, std::string_view("Rar!\x1a\x07\x01\0", 8), 100, nullptr},
        {"rar", "RAR archive", "rar", Cat::ARCHIVE, 0, std::string_view("Rar!\x1a\x07\0", 7), 100, nullptr},
        {"7z", "7-Zip archive", "7z", Cat::ARCHIVE, 0, std::string_view("7z\xbc\xaf\x27\x1c", 6), 100, nullptr},
        {"gz", "gzip data", "gz", Cat::ARCHIVE, 0, std::string_view("\x1f\x8b", 2), 95, &IsGzip},
        {"bz2", "bzip2 data", "bz2", Cat::ARCHIVE, 0, "BZh", 100, &IsBz2},
        {"xz", "XZ data", "xz", Cat::ARCHIVE, 0, std::string_view("\xfd" "7zXZ\0", 6), 100, nullptr},
        {"zst", "Zstandard data", "zst", Cat::ARCHIVE, 0, std::string_view("\x28\xb5\x2f\xfd", 4), 100, nullptr},
        {"lz4", "LZ4 data", "lz4", Cat::ARCHIVE, 0, std::string_view("\x04\x22\x4d\x18", 4), 100, nullptr},
        {"tar", "tar archive", "tar", Cat::ARCHIVE, 257, "ustar", 100, nullptr},
        {"cab", "Microsoft cabinet", "cab", Cat::ARCHIVE, 0, std::string_view("MSCF\0\0\0\0", 8), 100, nullptr},
        {"lzh", "LHA archive", "lzh", Cat::ARCHIVE, 2, "-l", 90, &IsLzh},
        {"arj", "ARJ archive", "arj", Cat::ARCHIVE, 0, std::string_view("\x60\xea", 2), 70, &IsArj},
        {"z", "Unix compress data", "z", Cat::ARCHIVE, 0, std::string_view("\x1f\x9d", 2), 80, nullptr},
        {"mpq", "Blizzard MPQ archive", "mpq", Cat::ARCHIVE, 0, "MPQ", 100, &IsMpq},
        // archives of the games
        {"pak", "Quake PAK archive", "pak", Cat::ARCHIVE, 0, "PACK", 90, &IsPackFile},
        {"wad", "Doom IWAD", "wad", Cat::ARCHIVE, 0, "IWAD", 95, &IsDoomWad},
        {"wad", "Doom PWAD", "wad", Cat::ARCHIVE, 0, "PWAD", 95, &IsDoomWad},
        {"wad2", "Quake WAD2 archive", "wad", Cat::ARCHIVE, 0, "WAD2", 100, nullptr},
        {"wad3", "Half-Life WAD3 archive", "wad", Cat::ARCHIVE, 0, "WAD3", 100, nullptr},
        {"gob", "Dark Forces GOB archive", "gob", Cat::ARCHIVE, 0, "GOB\n", 100, nullptr},
        {"grp", "Build GRP archive", "grp", Cat::ARCHIVE, 0, "KenSilverman", 100, nullptr},
        {"big", "EA BIG archive", "big", Cat::ARCHIVE, 0, "BIGF", 95, &IsBig},
        {"big", "EA BIG archive", "big", Cat::ARCHIVE, 0, "BIG4", 95, &IsBig},
        {"hog", "Descent HOG archive", "hog", Cat::ARCHIVE, 0, "DHF", 70, nullptr},
        {"hog2", "Descent 3 HOG archive", "hog", Cat::ARCHIVE, 0, "HOG2", 100, nullptr},
        {"vp", "Volition VP archive", "vp", Cat::ARCHIVE, 0, "VPVP", 95, &IsVp},
        {"rff", "Blood RFF archive", "rff", Cat::ARCHIVE, 0, "RFF\x1a", 100, nullptr},
        {"hpi", "Total Annihilation HPI archive", "hpi", Cat::ARCHIVE, 0, "HAPI", 100, nullptr},
        {"forge", "Assassin's Creed FORGE archive", "forge", Cat::ARCHIVE, 0, std::string_view("scimitar\0", 9), 100, nullptr},
        {"mos", "Enclave / Riddick MOS datafile", "xwc", Cat::ARCHIVE, 0, "MOS DATAFILE2.0", 100, nullptr},
        {"asura", "Asura archive", "asr", Cat::ARCHIVE, 0, "Asura   ", 100, nullptr},
        {"asura", "Asura archive (compressed)", "asr", Cat::ARCHIVE, 0, "AsuraZlb", 100, nullptr},
        {"bag", "Emperor / Nox BAG archive", "bag", Cat::ARCHIVE, 0, "GABA", 100, nullptr},
        {"lhab", "Lionhead audio bank", "lug", Cat::AUDIO, 0, "LiOnHeAd", 100, nullptr},
        {"spe", "Abuse resource file (SPEC)", "spe", Cat::ARCHIVE, 0, std::string_view("SPEC1.0\0", 8), 100, nullptr},
        {"dpa", "Drake Paint archive (DPA)", "dpa", Cat::ARCHIVE, 0, "DRAKEARC", 100, nullptr},
        {"duht", "Dragon UnPACKer list template", "uht", Cat::DATA, 0, "DUHT\x1a", 100, nullptr},
        {"hrf", "HyperRipper file", "hrf", Cat::DATA, 0, "HRFi\x1a", 100, nullptr},
        {"tgp0", "TGP archive", "tgp", Cat::ARCHIVE, 0, "TGP0", 100, nullptr},
        {"res", "Rage of Mages 2 RES archive", "res", Cat::ARCHIVE, 0, "&YA1", 100, nullptr},
        {"wad", "Geometry Wars DAWL archive", "wad", Cat::ARCHIVE, 0, "DAWL", 100, nullptr},
        {"unityfs", "Unity asset bundle (UnityFS)", "bundle", Cat::ARCHIVE, 0, std::string_view("UnityFS\0", 8), 100, nullptr},
        {"unityweb", "Unity asset bundle (UnityWeb)", "unity3d", Cat::ARCHIVE, 0, std::string_view("UnityWeb\0", 9), 100, nullptr},
        {"unityweb", "Unity asset bundle (UnityRaw)", "unity3d", Cat::ARCHIVE, 0, std::string_view("UnityRaw\0", 9), 100, nullptr},
        {"xnb", "XNA content (XNB)", "xnb", Cat::DATA, 0, "XNB", 95, &IsXnb},
        {"upk", "Unreal engine package", "upk", Cat::ARCHIVE, 0, std::string_view("\xc1\x83\x2a\x9e", 4), 100, nullptr},
        {"gdpc", "Godot PCK archive", "pck", Cat::ARCHIVE, 0, "GDPC", 100, nullptr},
        {"sarc", "Nintendo SARC archive", "sarc", Cat::ARCHIVE, 0, "SARC", 100, nullptr},
        {"yaz0", "Nintendo Yaz0 compressed data", "szs", Cat::ARCHIVE, 0, "Yaz0", 100, nullptr},
        {"u8", "Nintendo U8 archive", "arc", Cat::ARCHIVE, 0, std::string_view("\x55\xaa\x38\x2d", 4), 100, nullptr},
        // fonts
        {"otf", "OpenType font (CFF)", "otf", Cat::FONT, 0, "OTTO", 95, nullptr},
        {"ttf", "TrueType font", "ttf", Cat::FONT, 0, std::string_view("\0\1\0\0", 4), 85, &IsTtf},
        {"ttf", "TrueType font", "ttf", Cat::FONT, 0, "true", 90, &IsTtf},
        {"ttc", "TrueType collection", "ttc", Cat::FONT, 0, "ttcf", 95, nullptr},
        {"woff", "WOFF font", "woff", Cat::FONT, 0, "wOFF", 100, nullptr},
        {"woff2", "WOFF2 font", "woff2", Cat::FONT, 0, "wOF2", 100, nullptr},
        {"bmfont", "BMFont binary descriptor", "fnt", Cat::FONT, 0, std::string_view("BMF\x03", 4), 100, nullptr},
        // models and levels
        {"bsp", "Quake / Half-Life level (BSP)", "bsp", Cat::MODEL, 0, std::string_view("\x1d\0\0\0", 4), 90, &IsQuakeBsp},
        {"bsp", "Quake / Half-Life level (BSP)", "bsp", Cat::MODEL, 0, std::string_view("\x1e\0\0\0", 4), 90, &IsQuakeBsp},
        {"md2", "Quake 2 model (MD2)", "md2", Cat::MODEL, 0, "IDP2", 100, nullptr},
        {"md3", "Quake 3 model (MD3)", "md3", Cat::MODEL, 0, "IDP3", 100, nullptr},
        {"mdl", "Quake model (MDL)", "mdl", Cat::MODEL, 0, "IDPO", 100, nullptr},
        {"spr", "Quake sprite", "spr", Cat::IMAGE, 0, "IDSP", 100, nullptr},
        {"bsp", "Quake 2/3 level (BSP)", "bsp", Cat::MODEL, 0, "IBSP", 100, nullptr},
        {"bsp", "Source engine level (BSP)", "bsp", Cat::MODEL, 0, "VBSP", 100, nullptr},
        {"fbx", "Autodesk FBX (binary)", "fbx", Cat::MODEL, 0, std::string_view("Kaydara FBX Binary  \0", 21), 100, nullptr},
        {"glb", "glTF binary", "glb", Cat::MODEL, 0, "glTF", 100, nullptr},
        // programs and code
        {"elf", "ELF executable or library", "", Cat::CODE, 0, std::string_view("\x7f" "ELF", 4), 100, nullptr},
        {"exe", "Windows executable", "exe", Cat::CODE, 0, "MZ", 95, &IsPe},
        {"class", "Java class", "class", Cat::CODE, 0, std::string_view("\xca\xfe\xba\xbe", 4), 90, &IsJavaOrMachO},
        {"macho", "Mach-O universal binary", "", Cat::CODE, 0, std::string_view("\xca\xfe\xba\xbe", 4), 85, nullptr},
        {"macho", "Mach-O binary", "", Cat::CODE, 0, std::string_view("\xfe\xed\xfa\xce", 4), 100, nullptr},
        {"macho", "Mach-O binary", "", Cat::CODE, 0, std::string_view("\xfe\xed\xfa\xcf", 4), 100, nullptr},
        {"macho", "Mach-O binary", "", Cat::CODE, 0, std::string_view("\xce\xfa\xed\xfe", 4), 100, nullptr},
        {"macho", "Mach-O binary", "", Cat::CODE, 0, std::string_view("\xcf\xfa\xed\xfe", 4), 100, nullptr},
        {"luac", "Lua bytecode", "luac", Cat::CODE, 0, "\x1bLua", 100, nullptr},
        {"luajit", "LuaJIT bytecode", "ljbc", Cat::CODE, 0, "\x1bLJ", 100, nullptr},
        {"wasm", "WebAssembly module", "wasm", Cat::CODE, 0, std::string_view("\0asm", 4), 100, nullptr},
        {"dex", "Android Dalvik executable", "dex", Cat::CODE, 0, std::string_view("dex\n", 4), 100, nullptr},
        // documents and databases
        {"pdf", "PDF document", "pdf", Cat::DOCUMENT, 0, "%PDF-", 100, nullptr},
        {"rtf", "Rich Text document", "rtf", Cat::DOCUMENT, 0, "{\\rtf", 100, nullptr},
        {"ps", "PostScript document", "ps", Cat::DOCUMENT, 0, "%!PS", 100, nullptr},
        {"chm", "Compiled HTML help", "chm", Cat::DOCUMENT, 0, "ITSF", 100, nullptr},
        {"ole", "Microsoft OLE2 document", "doc", Cat::DOCUMENT, 0, std::string_view("\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1", 8), 100, nullptr},
        {"sqlite", "SQLite database", "sqlite", Cat::DATA, 0, std::string_view("SQLite format 3\0", 16), 100, nullptr},
    };
    return table;
}

// ---- containers with sub types -------------------------------------------------------------------------------------

auto IdentifyRiff(const Head &h) -> std::optional<DetectedType>
{
    if (!h.Is(0, "RIFF") && !h.Is(0, "RIFX") && !h.Is(0, "RF64"))
    {
        return std::nullopt;
    }
    const std::string form = h.Text(8, 4);
    if (form == "WAVE")
    {
        return Make("wav", "WAV audio", "wav", Cat::AUDIO, 100);
    }
    if (form == "AVI ")
    {
        return Make("avi", "AVI video", "avi", Cat::VIDEO, 100);
    }
    if (form == "WEBP")
    {
        return Make("webp", "WebP image", "webp", Cat::IMAGE, 100);
    }
    if (form == "RMID")
    {
        return Make("rmi", "RIFF MIDI music", "rmi", Cat::AUDIO, 100);
    }
    if (form == "PAL ")
    {
        return Make("pal", "RIFF palette", "pal", Cat::DATA, 100);
    }
    if (form == "ACON")
    {
        return Make("ani", "Animated cursor", "ani", Cat::IMAGE, 100);
    }
    if (form == "CDDA")
    {
        return Make("cda", "CD audio track", "cda", Cat::AUDIO, 100);
    }
    if (form == "mmRC")
    {
        return Make("mrc", "The Fifth Element MRC archive", "mrc", Cat::ARCHIVE, 100);
    }
    return Make("riff", "RIFF data", "riff", Cat::DATA, 70);
}

auto IdentifyIff(const Head &h) -> std::optional<DetectedType>
{
    if (!h.Is(0, "FORM"))
    {
        return std::nullopt;
    }
    const std::string form = h.Text(8, 4);
    if (form == "ILBM" || form == "PBM ")
    {
        return Make("lbm", "IFF interleaved bitmap", "lbm", Cat::IMAGE, 100);
    }
    if (form == "AIFF" || form == "AIFC")
    {
        return Make("aiff", "AIFF audio", "aif", Cat::AUDIO, 100);
    }
    if (form == "8SVX")
    {
        return Make("8svx", "IFF 8SVX audio", "8svx", Cat::AUDIO, 100);
    }
    if (form == "WVQA")
    {
        return Make("vqa", "Westwood VQA video", "vqa", Cat::VIDEO, 100);
    }
    return Make("iff", "IFF data", "iff", Cat::DATA, 60);
}

auto IdentifyOgg(const Head &h) -> std::optional<DetectedType>
{
    if (!h.Is(0, "OggS") || h.U8(4) != 0)
    {
        return std::nullopt;
    }
    if (h.Is(28, "\x01vorbis"))
    {
        return Make("ogg", "Ogg Vorbis audio", "ogg", Cat::AUDIO, 100);
    }
    if (h.Is(28, "OpusHead"))
    {
        return Make("opus", "Ogg Opus audio", "opus", Cat::AUDIO, 100);
    }
    if (h.Is(28, "\x7f" "FLAC"))
    {
        return Make("oga", "Ogg FLAC audio", "oga", Cat::AUDIO, 100);
    }
    if (h.Is(28, "\x80theora"))
    {
        return Make("ogv", "Ogg Theora video", "ogv", Cat::VIDEO, 100);
    }
    return Make("ogg", "Ogg stream", "ogg", Cat::AUDIO, 90);
}

auto IdentifyIsoMedia(const Head &h) -> std::optional<DetectedType>
{
    if (!h.Is(4, "ftyp"))
    {
        // QuickTime files without ftyp start with another atom
        if (h.Has(8, 4) && (h.Is(4, "moov") || h.Is(4, "mdat") || h.Is(4, "wide") || h.Is(4, "free")) && h.U32Be(0) >= 8)
        {
            return Make("mov", "QuickTime movie", "mov", Cat::VIDEO, 75);
        }
        return std::nullopt;
    }
    const std::string brand = h.Text(8, 4);
    if (brand == "M4A " || brand == "M4B " || brand == "M4P ")
    {
        return Make("m4a", "MPEG-4 audio", "m4a", Cat::AUDIO, 100);
    }
    if (brand == "qt  ")
    {
        return Make("mov", "QuickTime movie", "mov", Cat::VIDEO, 100);
    }
    if (brand == "heic" || brand == "heix" || brand == "mif1" || brand == "avif")
    {
        return Make(brand == "avif" ? "avif" : "heic", brand == "avif" ? "AVIF image" : "HEIF image", brand == "avif" ? "avif" : "heic", Cat::IMAGE, 100);
    }
    if (brand == "3gp4" || brand == "3gp5" || brand == "3gp6")
    {
        return Make("3gp", "3GPP video", "3gp", Cat::VIDEO, 100);
    }
    return Make("mp4", "MPEG-4 video", "mp4", Cat::VIDEO, 100);
}

auto IdentifyZip(const Head &h) -> std::optional<DetectedType>
{
    if (!h.Is(0, std::string_view("PK\3\4", 4)))
    {
        if (h.Is(0, std::string_view("PK\5\6", 4)))
        {
            return Make("zip", "ZIP archive (empty)", "zip", Cat::ARCHIVE, 95);
        }
        if (h.Is(0, std::string_view("PK\7\x08", 4)))
        {
            return Make("zip", "ZIP archive (spanned)", "zip", Cat::ARCHIVE, 90);
        }
        return std::nullopt;
    }
    const auto length = static_cast<std::size_t>(std::max<std::int64_t>(h.U16(26), 0));
    const std::string first = h.Text(30, length);
    if (first == "mimetype")
    {
        const std::string type = h.Text(30 + length, 40);
        if (type.find("epub") != std::string::npos)
        {
            return Make("epub", "EPUB book", "epub", Cat::DOCUMENT, 95);
        }
        if (type.find("opendocument") != std::string::npos)
        {
            return Make("odf", "OpenDocument file", "odt", Cat::DOCUMENT, 95);
        }
    }
    if (first == "[Content_Types].xml" || first.rfind("word/", 0) == 0 || first.rfind("xl/", 0) == 0 || first.rfind("ppt/", 0) == 0)
    {
        return Make("ooxml", "Office Open XML document", "docx", Cat::DOCUMENT, 90);
    }
    if (first == "AndroidManifest.xml")
    {
        return Make("apk", "Android package", "apk", Cat::ARCHIVE, 90);
    }
    if (first.rfind("META-INF/", 0) == 0)
    {
        return Make("jar", "Java archive", "jar", Cat::ARCHIVE, 90);
    }
    return Make("zip", "ZIP archive", "zip", Cat::ARCHIVE, 95);
}

auto IdentifyPe(const Head &h) -> std::optional<DetectedType>
{
    if (!h.Is(0, "MZ"))
    {
        return std::nullopt;
    }
    const auto pe = h.U32(0x3C);
    if (pe < 0x40 || pe + 26 > static_cast<std::int64_t>(h.Size()) || !h.Is(static_cast<std::size_t>(pe), std::string_view("PE\0\0", 4)))
    {
        return std::nullopt; // an old executable of DOS: the table says
    }
    const auto characteristics = h.U16(static_cast<std::size_t>(pe) + 22);
    if ((characteristics & 0x2000) != 0)
    {
        return Make("dll", "Windows library (DLL)", "dll", Cat::CODE, 95);
    }
    return Make("exe", "Windows executable", "exe", Cat::CODE, 95);
}

auto IdentifyEbmlKind(const Head &h) -> std::optional<DetectedType>
{
    if (!h.Is(0, std::string_view("\x1a\x45\xdf\xa3", 4)))
    {
        return std::nullopt;
    }
    const std::size_t limit = std::min<std::size_t>(h.Size(), 64);
    for (std::size_t i = 4; i + 4 <= limit; ++i)
    {
        if (h.Is(i, "webm"))
        {
            return Make("webm", "WebM video", "webm", Cat::VIDEO, 100);
        }
    }
    return Make("mkv", "Matroska video", "mkv", Cat::VIDEO, 100);
}

// ---- text and the rest -----------------------------------------------------------------------------------------------

auto Entropy(const Head &h) -> double
{
    std::array<std::size_t, 256> counts{};
    for (const std::byte b : h.Bytes())
    {
        ++counts[static_cast<std::uint8_t>(b)];
    }
    double entropy = 0;
    const double n = static_cast<double>(h.Size());
    for (const auto c : counts)
    {
        if (c != 0)
        {
            const double p = static_cast<double>(c) / n;
            entropy -= p * std::log2(p);
        }
    }
    return entropy;
}

auto Lower(std::string text) -> std::string
{
    for (auto &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

auto FirstWords(const std::string &text) -> std::string
{
    std::size_t start = 0;
    while (start < text.size() && (std::isspace(static_cast<unsigned char>(text[start])) != 0))
    {
        ++start;
    }
    return Lower(text.substr(start, 80));
}

auto IdentifyText(const Head &h) -> std::optional<DetectedType>
{
    if (h.Size() < 2)
    {
        return std::nullopt;
    }
    std::string text;
    std::string encoding = "ASCII";
    const auto bytes = h.Bytes();
    std::size_t at = 0;
    bool utf16 = false;
    if (h.U8(0) == 0xEF && h.U8(1) == 0xBB && h.U8(2) == 0xBF)
    {
        at = 3;
        encoding = "UTF-8";
    }
    else if ((h.U8(0) == 0xFF && h.U8(1) == 0xFE) || (h.U8(0) == 0xFE && h.U8(1) == 0xFF))
    {
        utf16 = true;
        encoding = "UTF-16";
    }
    if (utf16)
    {
        const bool little = h.U8(0) == 0xFF;
        for (std::size_t i = 2; i + 1 < h.Size(); i += 2)
        {
            const int lo = h.U8(little ? i : i + 1), hi = h.U8(little ? i + 1 : i);
            if (hi == 0 && (lo >= 32 || lo == '\n' || lo == '\r' || lo == '\t'))
            {
                text += static_cast<char>(lo);
            }
            else if (hi != 0)
            {
                text += '?';
            }
            else
            {
                return std::nullopt;
            }
        }
    }
    else
    {
        std::size_t high = 0, controls = 0;
        const std::size_t n = h.Size();
        for (std::size_t i = at; i < n; ++i)
        {
            const int c = h.U8(i);
            if (c == 0)
            {
                return std::nullopt;
            }
            if (c < 32 && c != '\n' && c != '\r' && c != '\t' && c != 0x1B && c != '\f')
            {
                ++controls;
            }
            if (c >= 0x80)
            {
                ++high;
            }
        }
        if (controls * 50 > n)
        {
            return std::nullopt; // more than 2% control characters
        }
        // UTF-8 (the end of the head may cut a character) or an 8 bit code page
        std::size_t i = at;
        bool valid = true;
        while (i < n)
        {
            const int c = h.U8(i);
            const int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
            if (extra < 0)
            {
                valid = false;
                break;
            }
            if (i + static_cast<std::size_t>(extra) >= n)
            {
                break;
            }
            for (int k = 1; k <= extra; ++k)
            {
                if ((h.U8(i + static_cast<std::size_t>(k)) & 0xC0) != 0x80)
                {
                    valid = false;
                }
            }
            if (!valid)
            {
                break;
            }
            i += static_cast<std::size_t>(extra) + 1;
        }
        if (high > 0)
        {
            encoding = valid ? "UTF-8" : "8 bit";
        }
        text.assign(reinterpret_cast<const char *>(bytes.data()) + at, n - at);
    }
    const std::string start = FirstWords(text);
    if (start.rfind("<?xml", 0) == 0)
    {
        return Make("xml", "XML document", "xml", Cat::TEXT, 90);
    }
    if (start.rfind("<!doctype html", 0) == 0 || start.rfind("<html", 0) == 0 || start.rfind("<head", 0) == 0 || start.rfind("<body", 0) == 0)
    {
        return Make("html", "HTML document", "html", Cat::TEXT, 90);
    }
    if (start.rfind("<svg", 0) == 0)
    {
        return Make("svg", "SVG image", "svg", Cat::IMAGE, 90);
    }
    if (start.rfind("#!", 0) == 0)
    {
        const std::string line = start.substr(0, start.find('\n'));
        if (line.find("python") != std::string::npos)
        {
            return Make("py", "Python script", "py", Cat::CODE, 85);
        }
        if (line.find("lua") != std::string::npos)
        {
            return Make("lua", "Lua script", "lua", Cat::CODE, 85);
        }
        if (line.find("perl") != std::string::npos)
        {
            return Make("pl", "Perl script", "pl", Cat::CODE, 85);
        }
        return Make("sh", "Shell script", "sh", Cat::CODE, 85);
    }
    if (start.rfind("ply\n", 0) == 0 || start.rfind("ply\r", 0) == 0)
    {
        return Make("ply", "PLY model (text)", "ply", Cat::MODEL, 90);
    }
    if (start.rfind("# wavefront", 0) == 0 || start.rfind("mtllib ", 0) == 0 || start.rfind("o ", 0) == 0 || start.rfind("v ", 0) == 0)
    {
        if (text.find("\nv ") != std::string::npos && text.find("\nf ") != std::string::npos)
        {
            return Make("obj", "Wavefront OBJ model", "obj", Cat::MODEL, 85);
        }
    }
    if (start.rfind("info face=", 0) == 0)
    {
        return Make("fnt", "BMFont text descriptor", "fnt", Cat::FONT, 90);
    }
    if (!start.empty() && (start.front() == '{' || start.front() == '[') && start.find('"') != std::string::npos && start.find(':') != std::string::npos)
    {
        return Make("json", "JSON data", "json", Cat::TEXT, 65);
    }
    if (!start.empty() && start.front() == '[' && start.find(']') != std::string::npos && text.find('=') != std::string::npos)
    {
        return Make("ini", "INI file", "ini", Cat::TEXT, 55);
    }
    DetectedType type = Make("text", "Text", "txt", Cat::TEXT, 50);
    type.Description = encoding == "ASCII" ? "Text" : "Text (" + encoding + ")";
    return type;
}

auto IdentifyTga(const Head &h) -> std::optional<DetectedType>
{
    if (!h.Has(0, 18))
    {
        return std::nullopt;
    }
    const int idLength = h.U8(0), mapType = h.U8(1), imageType = h.U8(2), bits = h.U8(16);
    const auto width = h.U16(12), height = h.U16(14);
    if (mapType > 1 || width < 1 || height < 1 || width > 8192 || height > 8192 || (h.U8(17) & 0xC0) != 0)
    {
        return std::nullopt;
    }
    const bool plain = imageType == 1 || imageType == 2 || imageType == 3;
    const bool rle = imageType == 9 || imageType == 10 || imageType == 11;
    if (!plain && !rle)
    {
        return std::nullopt;
    }
    if (bits != 8 && bits != 15 && bits != 16 && bits != 24 && bits != 32)
    {
        return std::nullopt;
    }
    if ((imageType == 1 || imageType == 9) != (mapType == 1))
    {
        return std::nullopt;
    }
    std::int64_t palette = 0;
    if (mapType == 1)
    {
        const auto entryBits = h.U8(7);
        if (entryBits != 15 && entryBits != 16 && entryBits != 24 && entryBits != 32)
        {
            return std::nullopt;
        }
        palette = h.U16(5) * ((entryBits + 7) / 8);
    }
    const std::int64_t pixels = width * height * ((bits + 7) / 8);
    const std::int64_t expected = 18 + idLength + palette + pixels;
    if (plain)
    {
        // the size of the file tells: with or without the footer of TGA 2
        if (h.FileSize() == expected || h.FileSize() == expected + 26)
        {
            return Make("tga", "Targa image", "tga", Cat::IMAGE, 85);
        }
        return std::nullopt;
    }
    return h.FileSize() >= 18 + idLength + palette + 2 && h.FileSize() <= expected + 26 + pixels / 2 ? std::optional<DetectedType>(Make("tga", "Targa image (RLE)", "tga", Cat::IMAGE, 60)) : std::nullopt;
}
} // namespace

auto GetCategoryName(FileCategory category) -> const char *
{
    switch (category)
    {
    case FileCategory::IMAGE:
        return "Image";
    case FileCategory::AUDIO:
        return "Audio";
    case FileCategory::VIDEO:
        return "Video";
    case FileCategory::ARCHIVE:
        return "Archive";
    case FileCategory::MODEL:
        return "3D model";
    case FileCategory::FONT:
        return "Font";
    case FileCategory::TEXT:
        return "Text";
    case FileCategory::DOCUMENT:
        return "Document";
    case FileCategory::CODE:
        return "Program";
    case FileCategory::DATA:
        return "Data";
    case FileCategory::UNKNOWN:
        break;
    }
    return "Unknown";
}

auto GetKnownTypes() -> std::vector<DetectedType>
{
    std::vector<DetectedType> types;
    for (const auto &s : Table())
    {
        types.push_back(Make(s.Id, s.Description, s.Extension, s.Category, s.Confidence));
    }
    return types;
}

auto SuggestExtension(const DetectedType &type) -> std::optional<std::string>
{
    if (type.Confidence < 60 || type.Extension.empty() || type.Category == FileCategory::UNKNOWN)
    {
        return std::nullopt;
    }
    return type.Extension;
}

auto IdentifyFile(std::span<const std::byte> bytes, std::int64_t size, std::string_view fileName) -> DetectedType
{
    const Head h(bytes, size);
    if (h.Size() == 0)
    {
        return Make("empty", "Empty file", "", Cat::UNKNOWN, 100);
    }
    // containers that tell what they hold in a second place
    for (const auto &special : {IdentifyRiff, IdentifyIff, IdentifyOgg, IdentifyIsoMedia, IdentifyZip, IdentifyEbmlKind, IdentifyPe})
    {
        if (const auto found = special(h))
        {
            return *found;
        }
    }
    for (const auto &s : Table())
    {
        if (h.Is(s.Offset, s.Magic))
        {
            const int confidence = s.Validate != nullptr ? s.Validate(h) : s.Confidence;
            if (confidence > 0)
            {
                return Make(s.Id, s.Description, s.Extension, s.Category, std::min(confidence, s.Confidence));
            }
        }
    }
    // a byte order mark: text before the frames of MPEG audio, which start with the same byte
    if ((h.U8(0) == 0xFF && h.U8(1) == 0xFE) || (h.U8(0) == 0xFE && h.U8(1) == 0xFF) || (h.U8(0) == 0xEF && h.U8(1) == 0xBB && h.U8(2) == 0xBF))
    {
        if (const auto text = IdentifyText(h))
        {
            return *text;
        }
    }
    // audio frames and structures without a signature
    if (const int mp3 = IsMpegAudio(h); mp3 > 0)
    {
        return Make("mp3", "MP3 audio", "mp3", Cat::AUDIO, mp3);
    }
    if (const int aac = IsAdts(h); aac > 0)
    {
        return Make("aac", "AAC audio (ADTS)", "aac", Cat::AUDIO, aac);
    }
    if (const int mod = IsMod(h); mod > 0)
    {
        return Make("mod", "Tracker module", "mod", Cat::AUDIO, mod);
    }
    if (const int dmx = IsDmxSound(h); dmx > 0)
    {
        return Make("dmx", "Doom sound (DMX)", "lmp", Cat::AUDIO, dmx);
    }
    if (const auto tga = IdentifyTga(h))
    {
        return *tga;
    }
    if (const auto text = IdentifyText(h))
    {
        return *text;
    }
    if (const int picture = IsDoomPicture(h); picture > 0)
    {
        return Make("doompic", "Doom picture", "lmp", Cat::IMAGE, picture);
    }
    if (h.Size() >= 512 && Entropy(h) > 7.5)
    {
        return Make("random", "High entropy data (compressed or encrypted)", "", Cat::UNKNOWN, 40);
    }
    // an extension is better than nothing
    std::string extension;
    if (const auto dot = fileName.rfind('.'); dot != std::string_view::npos && dot + 1 < fileName.size())
    {
        extension = Lower(std::string(fileName.substr(dot + 1)));
    }
    if (!extension.empty() && extension.size() <= 6)
    {
        DetectedType guessed = Make("ext", "", extension.c_str(), Cat::UNKNOWN, 20);
        guessed.Description = DescribeExtension(extension);
        guessed.Id = "ext-" + extension;
        return guessed;
    }
    return Make("data", "Binary data", "", Cat::UNKNOWN, 10);
}
} // namespace Dup::Core
