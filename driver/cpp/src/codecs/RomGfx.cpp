// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "RomGfx.h"

#include <algorithm>
#include <array>


namespace Dud
{
namespace
{
constexpr int64_t TilesPerRow = 16;
constexpr int64_t TgaHeader = 18;

auto BitsPerPixel(TileFormat format) -> int
{
    switch (format)
    {
    case TileFormat::Nes2bpp:
    case TileFormat::Snes2bpp:
        return 2;
    case TileFormat::Snes3bpp:
        return 3;
    default:
        return 4;
    }
}

// Palette index (0 .. 2^bpp - 1) of pixel x of row y of a tile
auto Pixel(const uint8_t *tile, TileFormat format, int x, int y) -> int
{
    const int bit = 7 - x;
    switch (format)
    {
    case TileFormat::Nes2bpp:
        return ((tile[y] >> bit) & 1) | (((tile[8 + y] >> bit) & 1) << 1);
    case TileFormat::Snes2bpp:
        return ((tile[2 * y] >> bit) & 1) | (((tile[2 * y + 1] >> bit) & 1) << 1);
    case TileFormat::Snes3bpp:
        return ((tile[2 * y] >> bit) & 1) | (((tile[2 * y + 1] >> bit) & 1) << 1) | (((tile[16 + y] >> bit) & 1) << 2);
    case TileFormat::Snes4bpp:
        return ((tile[2 * y] >> bit) & 1) | (((tile[2 * y + 1] >> bit) & 1) << 1) |
               (((tile[16 + 2 * y] >> bit) & 1) << 2) | (((tile[17 + 2 * y] >> bit) & 1) << 3);
    case TileFormat::Md4bpp: {
        const uint8_t value = tile[4 * y + x / 2];
        return (x & 1) ? (value & 15) : (value >> 4);
    }
    }
    return 0;
}

void Put16(std::vector<uint8_t> &out, size_t at, int value)
{
    out[at] = static_cast<uint8_t>(value & 255);
    out[at + 1] = static_cast<uint8_t>(value >> 8);
}
} // namespace

auto TileBytes(TileFormat format) -> int64_t
{
    return BitsPerPixel(format) * 8;
}

auto TileSheetSize(int64_t length, TileFormat format) -> int64_t
{
    const int64_t tiles = (length + TileBytes(format) - 1) / TileBytes(format);
    const int64_t rows = (tiles + TilesPerRow - 1) / TilesPerRow;
    return TgaHeader + TilesPerRow * 8 * rows * 8 * 4;
}

auto RenderTileSheet(const uint8_t *data, int64_t length, TileFormat format) -> std::vector<uint8_t>
{
    const int64_t tileBytes = TileBytes(format);
    const int64_t tiles = (length + tileBytes - 1) / tileBytes;
    const int64_t rows = (tiles + TilesPerRow - 1) / TilesPerRow;
    const int width = static_cast<int>(TilesPerRow * 8);
    const int height = static_cast<int>(rows * 8);
    std::vector<uint8_t> tga(static_cast<size_t>(TileSheetSize(length, format)), 0);
    tga[2] = 2;
    Put16(tga, 12, width);
    Put16(tga, 14, height);
    tga[16] = 32;
    tga[17] = 0x28; // 8 bits of alpha, rows from the top
    const int levels = (1 << BitsPerPixel(format)) - 1;
    std::array<uint8_t, 32> tile{};
    for (int64_t t = 0; t < tiles; ++t)
    {
        const int64_t start = t * tileBytes;
        const auto available = static_cast<size_t>(std::min(tileBytes, length - start));
        tile.fill(0);
        std::copy_n(data + start, available, tile.begin());
        const int64_t tx = (t % TilesPerRow) * 8;
        const int64_t ty = (t / TilesPerRow) * 8;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
            {
                const auto shade = static_cast<uint8_t>(Pixel(tile.data(), format, x, y) * 255 / levels);
                const size_t at = static_cast<size_t>(TgaHeader + ((ty + y) * width + tx + x) * 4);
                tga[at] = tga[at + 1] = tga[at + 2] = shade;
                tga[at + 3] = 255;
            }
    }
    return tga;
}

auto HeaderText(const uint8_t *field, size_t size) -> std::string
{
    std::string text;
    for (size_t i = 0; i < size; ++i)
        text += (field[i] >= 0x20 && field[i] < 0x7F) ? static_cast<char>(field[i]) : (field[i] == 0 ? ' ' : '?');
    const auto last = text.find_last_not_of(' ');
    text.erase(last == std::string::npos ? 0 : last + 1);
    const auto first = text.find_first_not_of(' ');
    return first == std::string::npos ? std::string() : text.substr(first);
}

auto InfoLine(const std::string &name, const std::string &value) -> std::string
{
    return name + ": " + value + "\n";
}

auto Hex(uint64_t value, int digits) -> std::string
{
    static constexpr char Digits[] = "0123456789ABCDEF";
    std::string text(static_cast<size_t>(digits), '0');
    for (int i = digits - 1; i >= 0 && value != 0; --i, value >>= 4)
        text[static_cast<size_t>(i)] = Digits[value & 15];
    return text;
}

auto RomSource::Size() const -> int64_t
{
    if (!_smd)
        return std::max<int64_t>(0, _stream.Size() - _base);
    return std::max<int64_t>(0, (_stream.Size() - SmdHeader) / SmdBlock) * SmdBlock;
}

auto RomSource::Read(int64_t offset, uint8_t *buffer, int64_t length) const -> int64_t
{
    std::fill_n(buffer, length, uint8_t{0});
    const int64_t size = Size();
    if (offset < 0 || offset >= size || length <= 0)
        return 0;
    length = std::min(length, size - offset);
    if (!_smd)
    {
        _stream.Seek(_base + offset);
        return static_cast<int64_t>(_stream.Read(buffer, static_cast<size_t>(length)));
    }
    std::array<uint8_t, SmdBlock> block;
    int64_t done = 0;
    while (done < length)
    {
        const int64_t position = offset + done;
        const int64_t number = position / SmdBlock;
        _stream.Seek(SmdHeader + number * SmdBlock);
        if (_stream.Read(block.data(), block.size()) != block.size())
            return done;
        const int64_t first = position % SmdBlock;
        const int64_t count = std::min(length - done, SmdBlock - first);
        for (int64_t i = 0; i < count; ++i)
        {
            const int64_t within = first + i;
            // even addresses are in the second half of the block
            buffer[done + i] = (within & 1) ? block[static_cast<size_t>(within >> 1)]
                                            : block[static_cast<size_t>(SmdBlock / 2 + (within >> 1))];
        }
        done += count;
    }
    return done;
}

void RomArchive::SetRom(int64_t base, bool interleaved)
{
    _rom = std::make_unique<RomSource>(*_stream, base, interleaved);
    _extractInternal = true;
}

void RomArchive::AddRaw(const std::string &name, int64_t offset, int64_t length)
{
    _plans.push_back({Kind::Raw, offset, length, TileFormat::Nes2bpp, {}});
    Add(name, _rom->Interleaved() ? offset : _rom->FileOffset(offset), length);
}

void RomArchive::AddFileBytes(const std::string &name, int64_t fileOffset, int64_t length)
{
    _plans.push_back({Kind::File, fileOffset, length, TileFormat::Nes2bpp, {}});
    Add(name, fileOffset, length);
}

void RomArchive::AddDecoded(const std::string &name, RomCodec codec, int64_t offset, int64_t packed, int64_t size,
                            bool sheet, TileFormat format)
{
    Plan plan;
    plan.Type = Kind::Decoded;
    plan.Offset = offset;
    plan.Length = size;
    plan.Format = format;
    plan.Codec = codec;
    plan.Packed = packed;
    plan.AsSheet = sheet;
    _plans.push_back(std::move(plan));
    if (!sheet)
        Add(name, offset, size);
    else
    {
        const int64_t tiles = (size + TileBytes(format) - 1) / TileBytes(format);
        Add(name, offset, TileSheetSize(size, format), TilesPerRow * 8, (tiles + TilesPerRow - 1) / TilesPerRow * 8);
    }
}

void RomArchive::AddText(const std::string &name, std::string text)
{
    const auto size = static_cast<int64_t>(text.size());
    _plans.push_back({Kind::Text, 0, size, TileFormat::Nes2bpp, std::move(text)});
    Add(name, 0, size);
}

void RomArchive::AddSheets(const std::string &folder, const std::string &prefix, int64_t offset, int64_t length,
                           TileFormat format, int64_t sheetBytes)
{
    for (int64_t at = 0; at < length; at += sheetBytes)
    {
        const int64_t part = std::min(sheetBytes, length - at);
        _plans.push_back({Kind::Sheet, offset + at, part, format, {}});
        const int64_t tiles = (part + TileBytes(format) - 1) / TileBytes(format);
        Add(folder + "/" + prefix + "_" + Hex(static_cast<uint64_t>(offset + at), 6) + ".tga", offset + at,
            TileSheetSize(part, format), TilesPerRow * 8, (tiles + TilesPerRow - 1) / TilesPerRow * 8);
    }
}

auto RomArchive::Extract(int64_t index, const Sink &sink) -> int32_t
{
    const Plan &plan = _plans[static_cast<size_t>(index)];
    switch (plan.Type)
    {
    case Kind::Text:
        return ExtractBytes(plan.Text.data(), static_cast<int64_t>(plan.Text.size()), sink);
    case Kind::Sheet: {
        std::vector<uint8_t> tiles(static_cast<size_t>(plan.Length));
        _rom->Read(plan.Offset, tiles.data(), plan.Length);
        const auto tga = RenderTileSheet(tiles.data(), plan.Length, plan.Format);
        return ExtractBytes(tga.data(), static_cast<int64_t>(tga.size()), sink);
    }
    case Kind::Decoded: {
        // a little more than the packed size: the end of the stream is found by the decoder
        std::vector<uint8_t> packed(static_cast<size_t>(plan.Packed));
        if (_rom->Read(plan.Offset, packed.data(), plan.Packed) != plan.Packed)
        {
            LastError = "Unexpected end of file";
            return DUDI7_ERR_IO;
        }
        std::vector<uint8_t> data;
        size_t used = 0;
        if (!Decode(plan.Codec, packed.data(), packed.size(), data, used) || static_cast<int64_t>(data.size()) != plan.Length)
        {
            LastError = "The compressed data cannot be decoded";
            return DUDI7_ERR_IO;
        }
        if (!plan.AsSheet)
            return ExtractBytes(data.data(), static_cast<int64_t>(data.size()), sink);
        const auto tga = RenderTileSheet(data.data(), static_cast<int64_t>(data.size()), plan.Format);
        return ExtractBytes(tga.data(), static_cast<int64_t>(tga.size()), sink);
    }
    case Kind::File: {
        std::vector<uint8_t> bytes(static_cast<size_t>(plan.Length));
        _stream->Seek(plan.Offset);
        if (_stream->Read(bytes.data(), bytes.size()) != bytes.size())
        {
            LastError = "Unexpected end of file";
            return DUDI7_ERR_IO;
        }
        return ExtractBytes(bytes.data(), plan.Length, sink);
    }
    case Kind::Raw:
        break;
    }
    std::vector<uint8_t> buffer(65536);
    for (int64_t done = 0; done < plan.Length;)
    {
        const int64_t chunk = std::min<int64_t>(plan.Length - done, static_cast<int64_t>(buffer.size()));
        if (_rom->Read(plan.Offset + done, buffer.data(), chunk) != chunk)
        {
            LastError = "Unexpected end of file";
            return DUDI7_ERR_IO;
        }
        if (!sink.Write(buffer.data(), chunk))
        {
            LastError = "Extraction stopped by the host";
            return DUDI7_ERR_CANCELLED;
        }
        done += chunk;
    }
    return DUDI7_OK;
}
} // namespace Dud
