// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ListTemplate.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace Dup::Core
{
namespace
{
constexpr std::size_t HEADER_SIZE = 16;
constexpr std::size_t EXTENDED_HEADER_SIZE = 8;
constexpr std::size_t ENTRY_SIZE = 15;
constexpr std::int32_t MAX_ENTRIES = 10000;
constexpr std::int64_t MAX_PART = 256LL * 1024 * 1024;

auto Lower(std::string text) -> std::string
{
    for (auto &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

auto Upper(std::string text) -> std::string
{
    for (auto &c : text)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return text;
}

auto Trim(const std::string &text) -> std::string
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

auto IsUtf8(const std::string &text) -> bool
{
    std::size_t i = 0;
    while (i < text.size())
    {
        const auto c = static_cast<unsigned char>(text[i]);
        const int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
        if (extra < 0 || i + static_cast<std::size_t>(extra) >= text.size())
        {
            return false;
        }
        for (int k = 1; k <= extra; ++k)
        {
            if ((static_cast<unsigned char>(text[i + static_cast<std::size_t>(k)]) & 0xC0) != 0x80)
            {
                return false;
            }
        }
        i += static_cast<std::size_t>(extra) + 1;
    }
    return true;
}

// The texts about a template were written for Windows: Latin-1 when they are not UTF-8
auto Utf8Of(const std::string &text) -> std::string
{
    if (IsUtf8(text))
    {
        return text;
    }
    std::string result;
    for (const char c : text)
    {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x80)
        {
            result += c;
        }
        else
        {
            result += static_cast<char>(0xC0 | (u >> 6));
            result += static_cast<char>(0x80 | (u & 0x3F));
        }
    }
    return result;
}

auto ToString(std::span<const std::byte> bytes) -> std::string
{
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

auto ToBytes(const std::string &text) -> std::vector<std::byte>
{
    std::vector<std::byte> bytes(text.size());
    std::memcpy(bytes.data(), text.data(), text.size());
    return bytes;
}

class Reader
{
  public:
    explicit Reader(std::span<const std::byte> data) : _data(data)
    {
    }
    [[nodiscard]] auto Pos() const -> std::size_t
    {
        return _pos;
    }
    void Seek(std::size_t position)
    {
        if (position > _data.size())
        {
            throw std::runtime_error("the template is truncated");
        }
        _pos = position;
    }
    auto U8() -> std::uint8_t
    {
        Need(1);
        return static_cast<std::uint8_t>(_data[_pos++]);
    }
    auto I32() -> std::int32_t
    {
        Need(4);
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
        {
            v |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(_data[_pos++])) << (8 * i);
        }
        return static_cast<std::int32_t>(v);
    }
    auto Bytes(std::size_t count) -> std::span<const std::byte>
    {
        Need(count);
        const auto result = _data.subspan(_pos, count);
        _pos += count;
        return result;
    }
    // length byte then the characters (cut at the first null, as v5 did)
    auto Text() -> std::string
    {
        const std::size_t length = U8();
        const auto bytes = Bytes(length);
        std::string text = ToString(bytes);
        const auto zero = text.find('\0');
        return zero == std::string::npos ? text : text.substr(0, zero);
    }

  private:
    void Need(std::size_t count) const
    {
        if (count > _data.size() - _pos)
        {
            throw std::runtime_error("the template is truncated");
        }
    }
    std::span<const std::byte> _data;
    std::size_t _pos{0};
};

void Put32(std::vector<std::byte> &out, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
    {
        out.push_back(static_cast<std::byte>(v >> (8 * i)));
    }
}

void PutText(std::vector<std::byte> &out, const std::string &text)
{
    const std::size_t length = std::min<std::size_t>(text.size(), 255);
    out.push_back(static_cast<std::byte>(length));
    for (std::size_t i = 0; i < length; ++i)
    {
        out.push_back(static_cast<std::byte>(text[i]));
    }
}

// The CRC32 of DUHTComp: the table of the standard one but without the 0xFFFFFFFF of the start and of the end
auto Crc32Raw(std::span<const std::byte> data) -> std::uint32_t
{
    static const auto table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i)
        {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
            {
                c = (c & 1) != 0 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
            }
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t crc = 0;
    for (const std::byte b : data)
    {
        crc = (crc >> 8) ^ table[(crc ^ static_cast<std::uint8_t>(b)) & 0xFF];
    }
    return crc;
}

auto Deflate(std::span<const std::byte> data) -> std::vector<std::byte>
{
    uLongf size = compressBound(static_cast<uLong>(data.size()));
    std::vector<std::byte> out(size);
    if (compress2(reinterpret_cast<Bytef *>(out.data()), &size, reinterpret_cast<const Bytef *>(data.data()), static_cast<uLong>(data.size()), 9) != Z_OK)
    {
        throw std::runtime_error("cannot compress a part of the template");
    }
    out.resize(size);
    return out;
}

auto Inflate(std::span<const std::byte> data, std::size_t size) -> std::vector<std::byte>
{
    std::vector<std::byte> out(size);
    uLongf produced = static_cast<uLongf>(size);
    if (uncompress(reinterpret_cast<Bytef *>(out.data()), &produced, reinterpret_cast<const Bytef *>(data.data()), static_cast<uLong>(data.size())) != Z_OK || produced != size)
    {
        throw std::runtime_error("a part of the template does not unpack");
    }
    return out;
}

auto FormatNumber(std::int64_t value, const std::string &separator) -> std::string
{
    std::string digits = std::to_string(value < 0 ? -value : value);
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i)
    {
        if (i > 0 && (digits.size() - i) % 3 == 0)
        {
            out += separator;
        }
        out += digits[i];
    }
    return value < 0 ? "-" + out : out;
}

auto Hex(std::uint64_t value, int digits) -> std::string
{
    static const char *const D = "0123456789ABCDEF";
    std::string out(static_cast<std::size_t>(digits), '0');
    for (int i = digits - 1; i >= 0 && value != 0; --i, value >>= 4)
    {
        out[static_cast<std::size_t>(i)] = D[value & 15];
    }
    return out;
}

// Replaces the macros ($$NAME$$, any case) of a text: the callback gives the value of a name, nullopt leaves the macro as it is
template <typename Resolver> auto ReplaceMacros(const std::string &text, Resolver &&resolve) -> std::string
{
    std::string out;
    std::size_t at = 0;
    while (at < text.size())
    {
        const auto start = text.find("$$", at);
        if (start == std::string::npos)
        {
            out.append(text, at, std::string::npos);
            break;
        }
        const auto end = text.find("$$", start + 2);
        if (end == std::string::npos)
        {
            out.append(text, at, std::string::npos);
            break;
        }
        out.append(text, at, start - at);
        const std::string name = text.substr(start + 2, end - start - 2);
        if (const auto value = resolve(name))
        {
            out += *value;
            at = end + 2;
        }
        else
        {
            out += "$$"; // not a macro: the second $$ may open one
            at = start + 2;
        }
    }
    return out;
}
} // namespace

auto ParseUht(std::span<const std::byte> bytes) -> ListTemplate
{
    Reader in(bytes);
    if (bytes.size() < HEADER_SIZE)
    {
        throw std::runtime_error("not a template");
    }
    const auto id = in.Bytes(5);
    const std::uint8_t version = in.U8();
    const std::uint8_t revision = in.U8();
    const std::int32_t entries = in.I32();
    const std::int32_t start = in.I32();
    const std::uint8_t extended = in.U8();
    if (std::memcmp(id.data(), "DUHT\x1A", 5) != 0 || version != 2 || entries <= 0 || entries > MAX_ENTRIES || start < static_cast<std::int32_t>(HEADER_SIZE) ||
        static_cast<std::size_t>(start) > bytes.size())
    {
        throw std::runtime_error("not a template of version 2");
    }
    if (extended == 1)
    {
        in.Bytes(EXTENDED_HEADER_SIZE);
    }
    ListTemplate result;
    result.Info.Name = Utf8Of(in.Text());
    result.Info.Author = Utf8Of(in.Text());
    result.Info.Email = Utf8Of(in.Text());
    result.Info.Version = Utf8Of(in.Text());
    result.Info.Url = Utf8Of(in.Text());
    if (revision >= 1)
    {
        result.Info.DefaultExtension = Utf8Of(in.Text());
        result.Info.ExtensionInfo = Utf8Of(in.Text());
    }
    else
    {
        result.Info.DefaultExtension = "XML";
        result.Info.ExtensionInfo = "eXtended Markup Language";
    }
    in.Seek(static_cast<std::size_t>(start));
    for (std::int32_t i = 0; i < entries; ++i)
    {
        const auto kind = in.Bytes(2);
        const std::int32_t size = in.I32();
        const std::int32_t fileSize = in.I32();
        const std::uint8_t compression = in.U8();
        in.I32(); // CRC32 (v5 did not check it)
        const std::string tag = ToString(kind);
        std::string name;
        if (tag == "IM")
        {
            name = in.Text();
        }
        if (size < 0 || fileSize < 0 || size > MAX_PART || fileSize > MAX_PART)
        {
            throw std::runtime_error("a part of the template is damaged");
        }
        const auto stored = in.Bytes(static_cast<std::size_t>(size));
        std::vector<std::byte> data;
        if (compression == 1)
        {
            data = Inflate(stored, static_cast<std::size_t>(fileSize));
        }
        else
        {
            data.assign(stored.begin(), stored.end());
        }
        if (tag == "HD")
        {
            result.Header = ToString(data);
        }
        else if (tag == "FT")
        {
            result.Footer = ToString(data);
        }
        else if (tag == "VR")
        {
            result.Variable = ToString(data);
        }
        else if (tag == "PV")
        {
            result.Preview = std::move(data);
        }
        else if (tag == "IM")
        {
            result.Images.push_back({name, std::move(data)});
        }
        // CM (comment), HR and FR (the root header and footer) are not used by the lists
    }
    return result;
}

auto CompileUht(const ListTemplate &list, bool compress) -> std::vector<std::byte>
{
    struct Part
    {
        const char *Id;
        std::string Name;
        std::vector<std::byte> Data;
    };
    std::vector<Part> parts;
    if (!list.Header.empty())
    {
        parts.push_back({"HD", {}, ToBytes(list.Header)});
    }
    if (!list.Footer.empty())
    {
        parts.push_back({"FT", {}, ToBytes(list.Footer)});
    }
    parts.push_back({"VR", {}, ToBytes(list.Variable)});
    if (!list.Preview.empty())
    {
        parts.push_back({"PV", {}, list.Preview});
    }
    for (const auto &image : list.Images)
    {
        parts.push_back({"IM", image.Name, image.Data});
    }

    std::vector<std::byte> out;
    const std::string signature = std::string("DUHT\x1A", 5);
    for (const char c : signature)
    {
        out.push_back(static_cast<std::byte>(c));
    }
    out.push_back(std::byte{2}); // version
    out.push_back(std::byte{1}); // revision
    Put32(out, static_cast<std::uint32_t>(parts.size()));
    const std::size_t startAt = out.size();
    Put32(out, 0);
    out.push_back(std::byte{1}); // extended header
    out.push_back(static_cast<std::byte>('D'));
    out.push_back(static_cast<std::byte>('U'));
    Put32(out, 20440); // the version of DUHTComp that this follows
    out.push_back(std::byte{3});
    out.push_back(std::byte{0}); // XOR key
    for (const auto *text : {&list.Info.Name, &list.Info.Author, &list.Info.Email, &list.Info.Version, &list.Info.Url, &list.Info.DefaultExtension, &list.Info.ExtensionInfo})
    {
        PutText(out, *text);
    }
    const auto first = static_cast<std::uint32_t>(out.size());
    for (std::size_t i = 0; i < 4; ++i)
    {
        out[startAt + i] = static_cast<std::byte>(first >> (8 * i));
    }
    for (const auto &part : parts)
    {
        std::vector<std::byte> packed;
        bool compressed = false;
        if (compress && !part.Data.empty())
        {
            packed = Deflate(part.Data);
            compressed = packed.size() < part.Data.size();
        }
        const auto &stored = compressed ? packed : part.Data;
        out.push_back(static_cast<std::byte>(part.Id[0]));
        out.push_back(static_cast<std::byte>(part.Id[1]));
        Put32(out, static_cast<std::uint32_t>(stored.size()));
        Put32(out, static_cast<std::uint32_t>(part.Data.size()));
        out.push_back(compressed ? std::byte{1} : std::byte{0});
        Put32(out, Crc32Raw(part.Data));
        if (std::string_view(part.Id) == "IM")
        {
            PutText(out, part.Name);
        }
        out.insert(out.end(), stored.begin(), stored.end());
    }
    return out;
}

auto ParseTsf(const std::string &text, const TemplateFileReader &reader) -> ListTemplate
{
    ListTemplate result;
    std::istringstream stream(text);
    std::string raw;
    int lineNumber = 0;
    bool inTsf = false;
    auto fail = [&](const std::string &message) {
        throw std::runtime_error("line " + std::to_string(lineNumber) + ": " + message);
    };
    auto read = [&](const std::string &name) {
        auto data = reader ? reader(name) : std::nullopt;
        if (!data)
        {
            fail("the file " + name + " does not exist");
        }
        return std::move(*data);
    };
    while (std::getline(stream, raw))
    {
        ++lineNumber;
        const std::string line = Trim(raw);
        if (line.empty() || line.front() == '#')
        {
            continue;
        }
        if (line.front() == '{')
        {
            const std::string tag = Upper(line);
            if (tag == "{TSF}")
            {
                inTsf = true;
            }
            else if (inTsf && tag == "{/TSF}")
            {
                inTsf = false;
            }
            else
            {
                fail("unknown tag or forbidden here - " + tag);
            }
            continue;
        }
        if (!inTsf)
        {
            fail("not a TSF file");
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos)
        {
            fail("equal (=) symbol not found but needed");
        }
        const std::string keyword = Upper(Trim(line.substr(0, equals)));
        const std::string value = Trim(line.substr(equals + 1));
        if (keyword == "NAME")
        {
            result.Info.Name = value;
        }
        else if (keyword == "URL")
        {
            result.Info.Url = value;
        }
        else if (keyword == "EMAIL")
        {
            result.Info.Email = value;
        }
        else if (keyword == "AUTHOR")
        {
            result.Info.Author = value;
        }
        else if (keyword == "VERSION")
        {
            result.Info.Version = value;
        }
        else if (keyword == "DEFAULTEXT")
        {
            result.Info.DefaultExtension = value;
        }
        else if (keyword == "EXTINFO")
        {
            result.Info.ExtensionInfo = value;
        }
        else if (keyword == "COMPRESSION")
        {
            result.Compress = value == "1" || Upper(value) == "TRUE";
        }
        else if (keyword == "OUTFILE")
        {
            // only for the compiler
        }
        else if (keyword == "HEADERFILE")
        {
            result.Header = ToString(read(value));
        }
        else if (keyword == "FOOTERFILE")
        {
            result.Footer = ToString(read(value));
        }
        else if (keyword == "VARIABLEFILE")
        {
            result.Variable = ToString(read(value));
        }
        else if (keyword == "PREVIEW")
        {
            result.Preview = read(value);
        }
        else if (keyword == "INCLUDEFILE")
        {
            const auto slash = value.find_last_of("/\\");
            result.Images.push_back({slash == std::string::npos ? value : value.substr(slash + 1), read(value)});
        }
        else
        {
            fail("unknown header keyword [" + keyword + "]");
        }
    }
    if (result.Variable.empty())
    {
        throw std::runtime_error("the template has no variable part (VariableFile)");
    }
    return result;
}

void SortEntries(std::vector<ListEntry> &entries, ListSort sort, bool invert)
{
    if (sort == ListSort::NONE)
    {
        return;
    }
    auto less = [&](const ListEntry &a, const ListEntry &b) {
        switch (sort)
        {
        case ListSort::NAME:
            return Lower(a.Name) < Lower(b.Name);
        case ListSort::SIZE:
            return a.Size < b.Size;
        default:
            return a.Offset < b.Offset;
        }
    };
    std::stable_sort(entries.begin(), entries.end(), [&](const ListEntry &a, const ListEntry &b) { return invert ? less(b, a) : less(a, b); });
}

auto GetDefaultDescriptionText(const std::string &keyword) -> std::string
{
    static const std::pair<const char *, const char *> texts[] = {
        {"EX", "File %e"},           {"EXANIM", "Animation (%e)"}, {"EXART", "Art Package (Textures)"}, {"EXBIN", "Data (%e)"},      {"EXCFG", "Configuration (%e)"},
        {"EXDLL", "Dynamic Link Library"}, {"EXFIRE", "Fire Texture (Dynamic)"}, {"EXIMG", "Image %e (%d)"}, {"EXMAP", "Level Map (%e)"}, {"EXMDL", "3D Model (%e)"},
        {"EXMPEG", "MPEG Audio/Video"},    {"EXMUS", "Music (%d)"},       {"EXPAL", "Palette"},              {"EXSCRP", "Script (%e)"},  {"EXSND", "Sound (%d)"},
        {"EXSPR", "Sprite"},               {"EXTEX", "Texture (%e)"},     {"EXTXT", "Text Document"},
    };
    for (const auto &[key, value] : texts)
    {
        if (keyword == key)
        {
            return value;
        }
    }
    return {};
}

auto DescribeExtension(const std::string &extension, const std::function<std::string(const std::string &)> &text) -> std::string
{
    const std::string ext = Upper(extension);
    auto get = [&](const std::string &keyword) {
        std::string value = text ? text(keyword) : std::string();
        return value.empty() ? GetDefaultDescriptionText(keyword) : value;
    };
    auto replace = [](std::string value, const std::string &token, const std::string &with) {
        for (auto at = value.find(token); at != std::string::npos; at = value.find(token, at + with.size()))
        {
            value.replace(at, token.size(), with);
        }
        return value;
    };
    auto image = [&](const std::string &name) { return replace(replace(get("EXIMG"), "%e", ext), "%d", name); };
    auto music = [&](const std::string &name) { return replace(get("EXMUS"), "%d", name); };
    auto sound = [&](const std::string &name) { return replace(get("EXSND"), "%d", name); };
    auto with = [&](const char *keyword) { return replace(get(keyword), "%e", ext); };
    if (ext == "ANI" || ext == "ANM")
    {
        return with("EXANIM");
    }
    if (ext == "ART")
    {
        return get("EXART");
    }
    if (ext == "BIN" || ext == "DAT")
    {
        return with("EXBIN");
    }
    if (ext == "BMP")
    {
        return image("Windows BitMaP");
    }
    if (ext == "BSP" || ext == "MAP")
    {
        return with("EXMAP");
    }
    if (ext == "CFG" || ext == "LST" || ext == "DEF")
    {
        return with("EXCFG");
    }
    if (ext == "CON")
    {
        return with("EXSCRP");
    }
    if (ext == "DLL" || ext == "LTO")
    {
        return get("EXDLL");
    }
    if (ext == "FIRE")
    {
        return get("EXFIRE");
    }
    if (ext == "IT")
    {
        return music("Impulse Tracker");
    }
    if (ext == "JPG" || ext == "JPEG")
    {
        return image("JPEG");
    }
    if (ext == "LMP" || ext == "DTX")
    {
        return with("EXTEX");
    }
    if (ext == "MID")
    {
        return music("MIDI");
    }
    if (ext == "MOD")
    {
        return music("Module");
    }
    if (ext == "MPG")
    {
        return get("EXMPEG");
    }
    if (ext == "MDL" || ext == "SKL" || ext == "ABC")
    {
        return with("EXMDL");
    }
    if (ext == "PAL")
    {
        return get("EXPAL");
    }
    if (ext == "PCX")
    {
        return image("Paintbrush PCX");
    }
    if (ext == "S3M")
    {
        return music("Scream Tracker");
    }
    if (ext == "SCC")
    {
        return replace(get("EXBIN"), "%e", "Visual SourceSafe");
    }
    if (ext == "SPR")
    {
        return get("EXSPR");
    }
    if (ext == "TGA")
    {
        return image("Targa");
    }
    if (ext == "TXT")
    {
        return get("EXTXT");
    }
    if (ext == "VOC")
    {
        return sound("Creative Voice");
    }
    if (ext == "WAV")
    {
        return sound("RIFF/WAVE");
    }
    if (ext == "XM")
    {
        return music("Extended Module");
    }
    // other well known types (not in v5)
    if (ext == "PNG")
    {
        return image("Portable Network Graphics");
    }
    if (ext == "GIF")
    {
        return image("GIF");
    }
    if (ext == "DDS")
    {
        return image("DirectDraw Surface");
    }
    if (ext == "OGG")
    {
        return sound("Ogg");
    }
    if (ext == "MP3")
    {
        return sound("MPEG audio");
    }
    return replace(get("EX"), "%e", ext);
}

auto RenderList(const ListTemplate &list, const ListContext &context, const std::vector<ListEntry> &entries) -> std::string
{
    std::int64_t totalBytes = 0;
    for (const auto &entry : entries)
    {
        totalBytes += entry.Size;
    }
    const auto general = [&](const std::string &name, const std::string &directory) -> std::optional<std::string> {
        const std::string key = Upper(name);
        if (key == "FILENAME")
        {
            return context.ArchiveName;
        }
        if (key == "DIRECTORY")
        {
            return directory;
        }
        if (key == "DUPVER")
        {
            return context.AppVersion;
        }
        if (key == "DUPEDIT")
        {
            return context.AppEdition;
        }
        if (key == "DUPURL")
        {
            return context.AppUrl;
        }
        if (key == "FORMAT")
        {
            return context.Format;
        }
        if (key.rfind("LNG<", 0) == 0 && key.size() > 4 && key.back() == '>')
        {
            const std::string keyword = key.substr(4, std::min<std::size_t>(6, key.size() - 5));
            return context.Language ? context.Language(keyword) : std::string();
        }
        if (key == "TOTNUMFILES")
        {
            return std::to_string(entries.size());
        }
        if (key == "TOTNUMBYTES")
        {
            return std::to_string(totalBytes);
        }
        if (key == "FTOTNUMFILES")
        {
            return FormatNumber(static_cast<std::int64_t>(entries.size()), context.ThousandsSeparator);
        }
        if (key == "FTOTNUMBYTES")
        {
            return FormatNumber(totalBytes, context.ThousandsSeparator);
        }
        return std::nullopt;
    };

    std::string out = ReplaceMacros(list.Header, [&](const std::string &name) { return general(name, context.Directory); });
    for (const auto &entry : entries)
    {
        std::string directory, file = entry.Name;
        if (!context.Separator.empty())
        {
            const auto at = entry.Name.rfind(context.Separator);
            if (at != std::string::npos)
            {
                directory = entry.Name.substr(0, at + context.Separator.size());
                file = entry.Name.substr(at + context.Separator.size());
            }
        }
        const auto dot = file.rfind('.');
        const std::string stem = dot == std::string::npos ? file : file.substr(0, dot);
        const std::string extension = dot == std::string::npos ? std::string() : file.substr(dot + 1);
        out += ReplaceMacros(list.Variable, [&](const std::string &name) -> std::optional<std::string> {
            const std::string key = Upper(name);
            if (key == "FILE")
            {
                return file;
            }
            if (key == "FILE0")
            {
                return stem;
            }
            if (key == "FILEEXT")
            {
                return extension;
            }
            if (key == "DESC")
            {
                return context.Describe ? context.Describe(extension) : DescribeExtension(extension);
            }
            if (key == "SIZE")
            {
                return std::to_string(entry.Size);
            }
            if (key == "FSIZE")
            {
                return FormatNumber(entry.Size, context.ThousandsSeparator);
            }
            if (key == "HSIZE")
            {
                return Hex(static_cast<std::uint64_t>(entry.Size), entry.Size > 0xFFFFFFFFLL ? 16 : 8);
            }
            if (key == "OFFSET")
            {
                return std::to_string(entry.Offset);
            }
            if (key == "FOFFSET")
            {
                return FormatNumber(entry.Offset, context.ThousandsSeparator);
            }
            if (key == "HOFFSET")
            {
                return Hex(static_cast<std::uint64_t>(entry.Offset), 16);
            }
            if (key == "DATAX")
            {
                return std::to_string(entry.DataX);
            }
            if (key == "DATAY")
            {
                return std::to_string(entry.DataY);
            }
            if (key == "HDATAX")
            {
                return Hex(static_cast<std::uint32_t>(entry.DataX), 8);
            }
            if (key == "HDATAY")
            {
                return Hex(static_cast<std::uint32_t>(entry.DataY), 8);
            }
            return general(name, directory);
        });
    }
    out += ReplaceMacros(list.Footer, [&](const std::string &name) { return general(name, context.Directory); });
    return out;
}
} // namespace Dup::Core
