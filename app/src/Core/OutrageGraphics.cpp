// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Descent 3 graphics formats, written from docs/file-formats-graphics-and-models.md of the Descent 3 repository:
// OGF (a TGA-like header with a name and a mip count, three-byte run packets of 16 bits samples), OAF (a list of OGF
// images) and the bitmap FNT fonts.

#include "Core/OutrageGraphics.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace Dup::Core
{
namespace
{
constexpr std::uint32_t FNT_SIGNATURE = 0xFEEDBABA;
constexpr std::size_t MAX_PIXELS = 1u << 26; // per level
constexpr unsigned MAX_OAF_FRAMES = 255;

class Failure : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

/// Bounds-checked little-endian reads.
class Cursor
{
  public:
    explicit Cursor(std::span<const std::byte> data) : _data(data)
    {
    }
    [[nodiscard]] auto Position() const -> std::size_t
    {
        return _pos;
    }
    [[nodiscard]] auto Remaining() const -> std::size_t
    {
        return _data.size() - _pos;
    }
    void Skip(std::size_t count)
    {
        Need(count);
        _pos += count;
    }
    auto U8() -> unsigned
    {
        Need(1);
        return static_cast<std::uint8_t>(_data[_pos++]);
    }
    auto U16() -> unsigned
    {
        const unsigned low = U8();
        return low | (U8() << 8);
    }
    auto I16() -> int
    {
        return static_cast<std::int16_t>(U16());
    }
    auto U32() -> std::uint32_t
    {
        const std::uint32_t low = U16();
        return low | (static_cast<std::uint32_t>(U16()) << 16);
    }
    auto I32() -> std::int32_t
    {
        return static_cast<std::int32_t>(U32());
    }
    auto F32() -> float
    {
        const std::uint32_t bits = U32();
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    /// A string through its first zero byte (consumed); at most maxLength bytes.
    auto ZString(std::size_t maxLength) -> std::string
    {
        std::string text;
        while (true)
        {
            const unsigned c = U8();
            if (c == 0)
            {
                return text;
            }
            if (text.size() >= maxLength)
            {
                throw Failure("unterminated name");
            }
            text += static_cast<char>(c);
        }
    }
    /// N bytes read as text up to the first zero.
    auto Fixed(std::size_t count) -> std::string
    {
        Need(count);
        std::string text;
        for (std::size_t i = 0; i < count; ++i)
        {
            const char c = static_cast<char>(_data[_pos + i]);
            if (c == 0)
            {
                break;
            }
            text += c;
        }
        _pos += count;
        return text;
    }
    auto Bytes(std::size_t count) -> std::span<const std::byte>
    {
        Need(count);
        const auto view = _data.subspan(_pos, count);
        _pos += count;
        return view;
    }

  private:
    void Need(std::size_t count) const
    {
        if (count > Remaining())
        {
            throw Failure("truncated data");
        }
    }
    std::span<const std::byte> _data;
    std::size_t _pos{0};
};

auto Expand5(unsigned v) -> std::uint8_t
{
    return static_cast<std::uint8_t>((v << 3) | (v >> 2));
}

void PutArgb1555(std::uint8_t *out, unsigned s)
{
    out[0] = Expand5((s >> 10) & 31);
    out[1] = Expand5((s >> 5) & 31);
    out[2] = Expand5(s & 31);
    out[3] = (s & 0x8000) ? 255 : 0;
}

void PutArgb4444(std::uint8_t *out, unsigned s)
{
    out[0] = static_cast<std::uint8_t>(((s >> 8) & 15) * 17);
    out[1] = static_cast<std::uint8_t>(((s >> 4) & 15) * 17);
    out[2] = static_cast<std::uint8_t>((s & 15) * 17);
    out[3] = static_cast<std::uint8_t>(((s >> 12) & 15) * 17);
}

/// Legacy RGB565: exactly 0x07E0 is transparent; the engine drops the lowest bit of green on conversion to 1555.
void PutRgb565(std::uint8_t *out, unsigned s)
{
    if (s == 0x07E0)
    {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    out[0] = Expand5((s >> 11) & 31);
    out[1] = Expand5((s >> 6) & 31);
    out[2] = Expand5(s & 31);
    out[3] = 255;
}

using PutPixel = void (*)(std::uint8_t *, unsigned);

auto DecodeOgfAt(Cursor &in) -> OgfImage
{
    OgfImage image;
    const unsigned idLength = in.U8();
    if (in.U8() != 0)
    {
        throw Failure("colour-mapped OGF images are not supported");
    }
    const unsigned type = in.U8();
    PutPixel put = nullptr;
    bool hasMips = true;
    switch (type)
    {
    case 121:
        put = &PutArgb4444;
        image.Name = in.ZString(1024);
        break;
    case 122:
        put = &PutArgb1555;
        image.Name = in.ZString(1024);
        break;
    case 123:
        put = &PutRgb565;
        image.Name = in.ZString(1024);
        break;
    case 124:
        put = &PutRgb565;
        image.Name = in.Fixed(35);
        break;
    case 127:
        put = &PutRgb565;
        image.Name = in.Fixed(35);
        hasMips = false;
        break;
    case 125:
        throw Failure("OGF type 125 (obsolete 8 bits) is not supported");
    case 126:
        throw Failure("OGF type 126 has no pixel decoder");
    default:
        throw Failure("not an OGF image type");
    }
    image.Type = static_cast<std::uint8_t>(type);
    const unsigned mips = hasMips ? in.U8() : 1;
    if (mips == 0)
    {
        throw Failure("OGF without mip levels");
    }
    in.Skip(9); // colour map specification and origin: ignored
    const unsigned width = in.U16(), height = in.U16();
    const unsigned depth = in.U8(), descriptor = in.U8();
    if (width == 0 || height == 0)
    {
        throw Failure("empty OGF image");
    }
    if (depth != 24 && depth != 32)
    {
        throw Failure("unexpected pixel depth");
    }
    if ((descriptor & 15) != 0 && (descriptor & 15) != 8)
    {
        throw Failure("unexpected image descriptor");
    }
    in.Skip(idLength);
    for (unsigned level = 0; level < mips; ++level)
    {
        const unsigned w = level < 16 ? width >> level : 0, h = level < 16 ? height >> level : 0;
        if (w == 0 || h == 0)
        {
            throw Failure("too many mip levels");
        }
        const std::size_t total = static_cast<std::size_t>(w) * h;
        if (total > MAX_PIXELS)
        {
            throw Failure("image too large");
        }
        DecodedImage level_image;
        level_image.Width = w;
        level_image.Height = h;
        level_image.Rgba.resize(total * 4);
        std::size_t done = 0;
        while (done < total)
        {
            const unsigned command = in.U8();
            const unsigned sample = in.U16();
            if (command == 1 || command > 250)
            {
                throw Failure("invalid run length");
            }
            const std::size_t count = command == 0 ? 1 : command;
            if (count > total - done)
            {
                throw Failure("a run goes past the end of its level");
            }
            std::uint8_t rgba[4];
            put(rgba, sample);
            for (std::size_t i = 0; i < count; ++i)
            {
                std::memcpy(&level_image.Rgba[(done + i) * 4], rgba, 4);
            }
            done += count;
        }
        image.Levels.push_back(std::move(level_image));
    }
    image.Consumed = in.Position();
    return image;
}

template <typename Function> auto Guarded(std::string *error, Function &&function) -> decltype(function())
{
    try
    {
        return function();
    }
    catch (const std::exception &failure)
    {
        if (error != nullptr)
        {
            *error = failure.what();
        }
        return {};
    }
}
} // namespace

auto LooksLikeOgf(std::span<const std::byte> data) -> bool
{
    if (data.size() < 20)
    {
        return false;
    }
    const auto type = static_cast<std::uint8_t>(data[2]);
    return data[1] == std::byte{0} && (type == 121 || type == 122 || type == 123 || type == 124 || type == 127);
}

auto DecodeOgf(std::span<const std::byte> data, std::string *error) -> std::optional<OgfImage>
{
    return Guarded(error, [&]() -> std::optional<OgfImage> {
        Cursor in(data);
        return DecodeOgfAt(in);
    });
}

auto DecodeOaf(std::span<const std::byte> data, std::string *error) -> std::optional<OafAnimation>
{
    return Guarded(error, [&]() -> std::optional<OafAnimation> {
        Cursor in(data);
        OafAnimation animation;
        const unsigned first = in.U8();
        unsigned count;
        if (first == 127)
        {
            animation.Version = static_cast<int>(in.U8());
            if (animation.Version != 1)
            {
                throw Failure("unsupported OAF version " + std::to_string(animation.Version));
            }
            count = in.U8();
            animation.FrameTime = in.F32();
        }
        else
        {
            animation.Legacy = true;
            count = first;
            in.F32();                    // play time
            animation.FrameTime = in.F32();
            in.I32();                    // flags
            in.F32();                    // light value
        }
        if (count == 0 || count > MAX_OAF_FRAMES)
        {
            throw Failure("no frames in the animation");
        }
        for (unsigned i = 0; i < count; ++i)
        {
            auto rest = data.subspan(in.Position());
            Cursor frame(rest);
            animation.Frames.push_back(DecodeOgfAt(frame));
            in.Skip(frame.Position());
        }
        return animation;
    });
}

auto IsFnt(std::span<const std::byte> data) -> bool
{
    return data.size() >= 46 && std::memcmp(data.data(), "\xBA\xBA\xED\xFE", 4) == 0;
}

auto DecodeFnt(std::span<const std::byte> data, std::string *error) -> std::optional<FntFont>
{
    return Guarded(error, [&]() -> std::optional<FntFont> {
        Cursor in(data);
        if (in.U32() != FNT_SIGNATURE)
        {
            throw Failure("not a FNT file");
        }
        FntFont font;
        font.Width = in.I16();
        font.Height = in.I16();
        font.Flags = in.U16();
        in.U16(); // baseline and brightness
        font.FirstChar = in.U8();
        font.LastChar = in.U8();
        font.Name = in.Fixed(32);
        if (font.LastChar < font.FirstChar || font.Height <= 0 || font.Width < 0)
        {
            throw Failure("invalid font header");
        }
        const std::size_t chars = font.LastChar - font.FirstChar + 1u;
        if (font.Flags & 0x20)
        {
            font.Tracking = in.I16();
            in.Skip(62);
        }
        for (std::size_t i = 0; i < chars; ++i)
        {
            font.Widths.push_back(font.Width);
        }
        if (font.Flags & 0x02)
        {
            for (std::size_t i = 0; i < chars; ++i)
            {
                font.Widths[i] = in.I16();
            }
        }
        if (font.Flags & 0x04)
        {
            const int kernCount = in.I16();
            if (kernCount < 0)
            {
                throw Failure("negative kerning count");
            }
            for (int i = 0; i < kernCount; ++i)
            {
                const auto a = static_cast<std::uint8_t>(in.U8());
                const auto b = static_cast<std::uint8_t>(in.U8());
                font.Kerns.push_back({a, b, static_cast<std::int8_t>(in.U8())});
            }
        }
        const std::int32_t length = in.I32();
        if (length < 0 || static_cast<std::size_t>(length) > in.Remaining())
        {
            throw Failure("glyph data goes past the end of the file");
        }
        Cursor pixels(in.Bytes(static_cast<std::size_t>(length)));
        const bool color = (font.Flags & 0x01) != 0;
        const bool argb4444 = (font.Flags & 0x10) != 0;
        const std::size_t height = static_cast<std::size_t>(font.Height);
        for (std::size_t i = 0; i < chars; ++i)
        {
            const int w = font.Widths[i];
            if (w < 0 || w > 255)
            {
                throw Failure("glyph width out of range");
            }
            DecodedImage glyph;
            glyph.Width = static_cast<std::uint32_t>(w);
            glyph.Height = static_cast<std::uint32_t>(height);
            glyph.Rgba.assign(static_cast<std::size_t>(w) * height * 4, 0);
            if (color)
            {
                for (std::size_t p = 0; p < static_cast<std::size_t>(w) * height; ++p)
                {
                    const unsigned s = pixels.U16();
                    if (argb4444)
                    {
                        PutArgb4444(&glyph.Rgba[p * 4], s);
                    }
                    else
                    {
                        PutRgb565(&glyph.Rgba[p * 4], s);
                    }
                }
            }
            else
            {
                const std::size_t rowBytes = (static_cast<std::size_t>(w) + 7) / 8;
                for (std::size_t y = 0; y < height; ++y)
                {
                    const auto row = pixels.Bytes(rowBytes);
                    for (int x = 0; x < w; ++x)
                    {
                        if (static_cast<unsigned>(row[static_cast<std::size_t>(x) / 8]) & (0x80u >> (x % 8)))
                        {
                            std::uint8_t *out = &glyph.Rgba[(y * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 4];
                            out[0] = out[1] = out[2] = out[3] = 255;
                        }
                    }
                }
            }
            font.Glyphs.push_back(std::move(glyph));
        }
        return font;
    });
}

namespace
{
/// Alpha-blends a picture over the sheet (opaque) at x, y.
void Blit(DecodedImage &sheet, const DecodedImage &source, std::uint32_t x0, std::uint32_t y0)
{
    for (std::uint32_t y = 0; y < source.Height && y0 + y < sheet.Height; ++y)
    {
        for (std::uint32_t x = 0; x < source.Width && x0 + x < sheet.Width; ++x)
        {
            const std::uint8_t *in = &source.Rgba[(static_cast<std::size_t>(y) * source.Width + x) * 4];
            std::uint8_t *out = &sheet.Rgba[(static_cast<std::size_t>(y0 + y) * sheet.Width + x0 + x) * 4];
            for (int c = 0; c < 3; ++c)
            {
                out[c] = static_cast<std::uint8_t>((in[c] * in[3] + out[c] * (255 - in[3])) / 255);
            }
        }
    }
}

auto Sheet(std::uint32_t columns, std::uint32_t rows, std::uint32_t cellW, std::uint32_t cellH, std::uint8_t background) -> DecodedImage
{
    DecodedImage sheet;
    sheet.Width = columns * cellW;
    sheet.Height = rows * cellH;
    sheet.Rgba.assign(static_cast<std::size_t>(sheet.Width) * sheet.Height * 4, background);
    for (std::size_t i = 3; i < sheet.Rgba.size(); i += 4)
    {
        sheet.Rgba[i] = 255;
    }
    return sheet;
}
} // namespace

auto MakeFrameSheet(const OafAnimation &animation) -> DecodedImage
{
    std::uint32_t cellW = 1, cellH = 1;
    for (const auto &frame : animation.Frames)
    {
        cellW = std::max(cellW, frame.Levels.front().Width);
        cellH = std::max(cellH, frame.Levels.front().Height);
    }
    const auto count = static_cast<std::uint32_t>(animation.Frames.size());
    std::uint32_t columns = 1;
    while (columns * columns < count)
    {
        ++columns;
    }
    const std::uint32_t rows = (count + columns - 1) / columns;
    auto sheet = Sheet(columns, rows, cellW, cellH, 40);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        Blit(sheet, animation.Frames[i].Levels.front(), (i % columns) * cellW, (i / columns) * cellH);
    }
    return sheet;
}

auto MakeGlyphSheet(const FntFont &font) -> DecodedImage
{
    std::uint32_t cellW = 1;
    for (const auto &glyph : font.Glyphs)
    {
        cellW = std::max(cellW, glyph.Width);
    }
    cellW += 2;
    const auto cellH = static_cast<std::uint32_t>(font.Height) + 2;
    const auto count = static_cast<std::uint32_t>(font.Glyphs.size());
    const std::uint32_t columns = std::min<std::uint32_t>(16, std::max<std::uint32_t>(count, 1));
    const std::uint32_t rows = (count + columns - 1) / columns;
    auto sheet = Sheet(columns, rows, cellW, cellH, 40);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        Blit(sheet, font.Glyphs[i], (i % columns) * cellW + 1, (i / columns) * cellH + 1);
    }
    return sheet;
}
} // namespace Dup::Core
