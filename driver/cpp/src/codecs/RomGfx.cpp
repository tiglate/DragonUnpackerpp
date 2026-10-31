// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "RomGfx.h"

#include <algorithm>
#include <array>

#include "codecs/Brr.h"
#include "codecs/Palette.h"
#include "codecs/Spc.h"

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
    case TileFormat::Snes8bppLinear:
        return 8;
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
    case TileFormat::Snes4bppLinear: {
        const uint8_t value = tile[4 * y + x / 2];
        return (x & 1) ? (value >> 4) : (value & 15);
    }
    case TileFormat::Snes8bppLinear:
        return tile[8 * y + x];
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

// The sheets are colour mapped TGA files (8 bits per pixel, a map of 16 grays): 4 times smaller than true
// colour, and a viewer that can change the map shows the tiles with the colors of a palette of the ROM.
constexpr int64_t MapColors = 16;
// Mode 7 sheets have 8 bits pixels: a map of 256 colours
auto MapEntries(TileFormat format) -> int64_t
{
    return format == TileFormat::Snes8bppLinear ? 256 : MapColors;
}

auto TileSheetSize(int64_t length, TileFormat format) -> int64_t
{
    const int64_t tiles = (length + TileBytes(format) - 1) / TileBytes(format);
    const int64_t rows = (tiles + TilesPerRow - 1) / TilesPerRow;
    return TgaHeader + MapEntries(format) * 3 + TilesPerRow * 8 * rows * 8;
}

auto RenderTileSheet(const uint8_t *data, int64_t length, TileFormat format, const std::vector<uint32_t> &colors) -> std::vector<uint8_t>
{
    const int64_t tileBytes = TileBytes(format);
    const int64_t tiles = (length + tileBytes - 1) / tileBytes;
    const int64_t rows = (tiles + TilesPerRow - 1) / TilesPerRow;
    const int width = static_cast<int>(TilesPerRow * 8);
    const int height = static_cast<int>(rows * 8);
    std::vector<uint8_t> tga(static_cast<size_t>(TileSheetSize(length, format)), 0);
    tga[1] = 1; // colour map
    tga[2] = 1; // colour mapped image
    const int64_t mapColors = MapEntries(format);
    Put16(tga, 5, static_cast<int>(mapColors));
    tga[7] = 24;
    Put16(tga, 12, width);
    Put16(tga, 14, height);
    tga[16] = 8;
    tga[17] = 0x20; // rows from the top
    const int levels = (1 << BitsPerPixel(format)) - 1;
    for (int i = 0; i < mapColors; ++i) // grays for the colors the format has, black after them
    {
        const auto shade = static_cast<uint8_t>(i <= levels ? i * 255 / levels : 0);
        tga[static_cast<size_t>(TgaHeader + i * 3)] = tga[static_cast<size_t>(TgaHeader + i * 3 + 1)] =
            tga[static_cast<size_t>(TgaHeader + i * 3 + 2)] = shade;
        if (i <= levels && static_cast<size_t>(i) < colors.size()) // TGA colour maps are blue, green, red
        {
            tga[static_cast<size_t>(TgaHeader + i * 3)] = static_cast<uint8_t>(colors[static_cast<size_t>(i)] & 255);
            tga[static_cast<size_t>(TgaHeader + i * 3 + 1)] = static_cast<uint8_t>((colors[static_cast<size_t>(i)] >> 8) & 255);
            tga[static_cast<size_t>(TgaHeader + i * 3 + 2)] = static_cast<uint8_t>((colors[static_cast<size_t>(i)] >> 16) & 255);
        }
    }
    const auto pixels = static_cast<size_t>(TgaHeader + mapColors * 3);
    std::array<uint8_t, 64> tile{};
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
                tga[pixels + static_cast<size_t>((ty + y) * width + tx + x)] = static_cast<uint8_t>(Pixel(tile.data(), format, x, y));
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
                            bool sheet, TileFormat format, const std::vector<uint32_t> &colors)
{
    Plan plan;
    plan.Type = Kind::Decoded;
    plan.Offset = offset;
    plan.Length = size;
    plan.Format = format;
    plan.Codec = codec;
    plan.Packed = packed;
    plan.AsSheet = sheet;
    plan.Colors = colors;
    _plans.push_back(std::move(plan));
    if (!sheet)
        Add(name, offset, size);
    else
    {
        const int64_t tiles = (size + TileBytes(format) - 1) / TileBytes(format);
        Add(name, offset, TileSheetSize(size, format), TilesPerRow * 8, (tiles + TilesPerRow - 1) / TilesPerRow * 8);
    }
}

void RomArchive::AddBrr(const std::string &name, int64_t offset, int64_t blocks)
{
    Plan plan;
    plan.Type = Kind::Brr;
    plan.Offset = offset;
    plan.Length = blocks * BrrBlockBytes;
    _plans.push_back(std::move(plan));
    Add(name, offset, BrrWavSize(blocks));
}

void RomArchive::AddPcm(const std::string &name, int64_t offset, int64_t length)
{
    Plan plan;
    plan.Type = Kind::Pcm;
    plan.Offset = offset;
    plan.Length = length;
    _plans.push_back(std::move(plan));
    Add(name, offset, PcmWavSize(length));
}

void RomArchive::AddPalettes(const uint8_t *image, size_t size, const std::vector<PaletteFind> &finds)
{
    for (const PaletteFind &find : finds)
    {
        const auto bytes = static_cast<size_t>(find.Colors) * static_cast<size_t>(BytesPerColor(find.Kind));
        if (find.Offset < 0 || static_cast<size_t>(find.Offset) + bytes > size)
            continue;
        const auto colors = PaletteColors(find.Kind, image + find.Offset, find.Colors);
        const std::string base = "palettes/pal_" + Hex(static_cast<uint64_t>(find.Offset), 6);
        const auto jasc = MakeJascPal(colors);
        AddText(base + ".pal", std::string(jasc.begin(), jasc.end()));
        Plan plan;
        plan.Type = Kind::Swatch;
        plan.Offset = find.Offset;
        plan.Colors = colors;
        _plans.push_back(std::move(plan));
        Add(base + ".tga", find.Offset, static_cast<int64_t>(MakeSwatchTga(colors).size()));
    }
}

void RomArchive::AddSpc(const std::string &name, std::shared_ptr<const KnownMusic> music, size_t bank, size_t song, const std::string &title,
                        const std::string &game)
{
    Plan plan;
    plan.Type = Kind::Spc;
    plan.Music = std::move(music);
    plan.Bank = bank;
    plan.Song = song;
    plan.Title = title;
    plan.Text = game;
    _plans.push_back(std::move(plan));
    Add(name, 0, SpcFileBytes);
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

void RomArchive::AddColoredSheets(const std::string &folder, const std::string &prefix, int64_t offset, int64_t length,
                                  TileFormat format, int64_t sheetBytes, const std::vector<uint32_t> &colors)
{
    for (int64_t at = 0; at < length; at += sheetBytes)
    {
        const int64_t part = std::min(sheetBytes, length - at);
        Plan plan{Kind::Sheet, offset + at, part, format, {}};
        plan.Colors = colors;
        _plans.push_back(std::move(plan));
        const int64_t tiles = (part + TileBytes(format) - 1) / TileBytes(format);
        Add(folder + "/" + prefix + "_" + Hex(static_cast<uint64_t>(offset + at), 6) + ".tga", offset + at,
            TileSheetSize(part, format), TilesPerRow * 8, (tiles + TilesPerRow - 1) / TilesPerRow * 8);
    }
}

void RomArchive::AddColorPalette(const std::string &base, const std::vector<uint32_t> &colors)
{
    const auto jasc = MakeJascPal(colors);
    AddText(base + ".pal", std::string(jasc.begin(), jasc.end()));
    Plan plan;
    plan.Type = Kind::Swatch;
    plan.Colors = colors;
    _plans.push_back(std::move(plan));
    Add(base + ".tga", 0, static_cast<int64_t>(MakeSwatchTga(colors).size()));
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
        const auto tga = RenderTileSheet(tiles.data(), plan.Length, plan.Format, plan.Colors);
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
        const auto tga = RenderTileSheet(data.data(), static_cast<int64_t>(data.size()), plan.Format, plan.Colors);
        return ExtractBytes(tga.data(), static_cast<int64_t>(tga.size()), sink);
    }
    case Kind::Pcm: {
        std::vector<uint8_t> samples(static_cast<size_t>(plan.Length));
        if (_rom->Read(plan.Offset, samples.data(), plan.Length) != plan.Length)
        {
            LastError = "Unexpected end of file";
            return DUDI7_ERR_IO;
        }
        const auto wav = MakePcmWav(samples.data(), samples.size(), PcmSampleRate);
        return ExtractBytes(wav.data(), static_cast<int64_t>(wav.size()), sink);
    }
    case Kind::Spc: {
        const auto file = MakeSpc(*plan.Music, plan.Bank, plan.Song, *_rom, plan.Title, plan.Text);
        return ExtractBytes(file.data(), static_cast<int64_t>(file.size()), sink);
    }
    case Kind::Swatch: {
        const auto tga = MakeSwatchTga(plan.Colors);
        return ExtractBytes(tga.data(), static_cast<int64_t>(tga.size()), sink);
    }
    case Kind::Brr: {
        std::vector<uint8_t> brr(static_cast<size_t>(plan.Length));
        if (_rom->Read(plan.Offset, brr.data(), plan.Length) != plan.Length)
        {
            LastError = "Unexpected end of file";
            return DUDI7_ERR_IO;
        }
        const auto wav = MakeWav(DecodeBrr(brr.data(), brr.size() / BrrBlockBytes), BrrSampleRate);
        return ExtractBytes(wav.data(), static_cast<int64_t>(wav.size()), sink);
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
