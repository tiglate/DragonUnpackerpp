// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Reader.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>

#include "dudi7.h"

namespace Dud
{
Reader::Reader(const std::string &utf8Path)
    : _path(utf8Path), _file(std::filesystem::path(std::u8string(utf8Path.begin(), utf8Path.end())), std::ios::binary),
      _cache(CacheSize)
{
    if (!_file)
        throw Error(DUDI7_ERR_IO, "Cannot open " + utf8Path);
    _file.seekg(0, std::ios::end);
    _fileSize = static_cast<int64_t>(_file.tellg());
    _file.seekg(0, std::ios::beg);
}

Reader::Reader(std::vector<uint8_t> data)
    : _inMemory(true), _memory(std::move(data)), _fileSize(static_cast<int64_t>(_memory.size()))
{
}

auto Reader::Read(void *buffer, size_t count) -> size_t
{
    auto *dst = static_cast<char *>(buffer);
    if (_inMemory)
    {
        if (_pos >= _fileSize)
            return 0;
        const size_t n = std::min(count, static_cast<size_t>(_fileSize - _pos));
        std::memcpy(dst, _memory.data() + _pos, n);
        _pos += static_cast<int64_t>(n);
        return n;
    }
    size_t total = 0;
    while (count > 0 && _pos < _fileSize)
    {
        size_t n = 0;
        if (_pos >= _cacheStart && _pos < _cacheStart + static_cast<int64_t>(_cacheLen))
        {
            n = std::min(count, static_cast<size_t>(_cacheStart + static_cast<int64_t>(_cacheLen) - _pos));
            std::memcpy(dst, _cache.data() + (_pos - _cacheStart), n);
        }
        else if (count >= CacheSize)
        {
            // big reads go straight to the file
            _file.clear();
            _file.seekg(_pos);
            _file.read(dst, static_cast<std::streamsize>(count));
            n = static_cast<size_t>(_file.gcount());
            if (n == 0)
                break;
        }
        else
        {
            _file.clear();
            _file.seekg(_pos);
            _file.read(_cache.data(), static_cast<std::streamsize>(CacheSize));
            _cacheStart = _pos;
            _cacheLen = static_cast<size_t>(_file.gcount());
            if (_cacheLen == 0)
                break;
            continue;
        }
        dst += n;
        _pos += static_cast<int64_t>(n);
        count -= n;
        total += n;
    }
    return total;
}

void Reader::ReadBuffer(void *buffer, size_t count)
{
    if (Read(buffer, count) != count)
        throw ReadError();
}

namespace
{
template <typename T> auto ReadLe(Reader &reader) -> T
{
    T value{};
    if (reader.Read(&value, sizeof(T)) != sizeof(T))
        throw Error(DUDI7_ERR_IO, "Unexpected end of file");
    return value;
}
} // namespace

auto Reader::U8() -> uint8_t
{
    return ReadLe<uint8_t>(*this);
}
auto Reader::U16() -> uint16_t
{
    return ReadLe<uint16_t>(*this);
}
auto Reader::U32() -> uint32_t
{
    return ReadLe<uint32_t>(*this);
}
auto Reader::I32() -> int32_t
{
    return ReadLe<int32_t>(*this);
}
auto Reader::U64() -> uint64_t
{
    return ReadLe<uint64_t>(*this);
}
auto Reader::I64() -> int64_t
{
    return ReadLe<int64_t>(*this);
}
auto Reader::BE32() -> uint32_t
{
    const uint32_t v = ReadLe<uint32_t>(*this);
    return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
}
auto Reader::BE16() -> uint16_t
{
    const uint16_t v = ReadLe<uint16_t>(*this);
    return static_cast<uint16_t>((v >> 8) | (v << 8));
}

auto Reader::CString(size_t maxLen) -> std::string
{
    std::string result;
    do
    {
        const char c = static_cast<char>(U8());
        if (c == '\0')
            return result;
        result += c;
    } while (result.size() <= maxLen);
    throw Error(DUDI7_ERR_NOTFORMAT, "Text too long");
}

auto Reader::LString32(size_t maxLen) -> std::string
{
    const uint32_t n = U32();
    if (n > maxLen)
        throw Error(DUDI7_ERR_NOTFORMAT, "Text too long");
    std::string text(n, '\0');
    if (n > 0 && Read(text.data(), n) != n)
        throw Error(DUDI7_ERR_IO, "Unexpected end of file");
    return ZString(text.data(), n);
}

auto Reader::WString32(size_t maxLen) -> std::string
{
    const uint32_t n = U32();
    if (n > maxLen)
        throw Error(DUDI7_ERR_NOTFORMAT, "Text too long");
    std::u16string wide(n, u'\0');
    if (n > 0 && Read(wide.data(), static_cast<size_t>(n) * 2) != static_cast<size_t>(n) * 2)
        throw Error(DUDI7_ERR_IO, "Unexpected end of file");
    return Utf16ToUtf8(wide);
}

auto ZString(const char *buffer, size_t size) -> std::string
{
    size_t n = 0;
    while (n < size && buffer[n] != '\0')
        ++n;
    return std::string(buffer, n);
}

auto StartsWith(Reader &reader, std::string_view signature) -> bool
{
    reader.Seek(0);
    std::string buffer(signature.size(), '\0');
    return reader.Read(buffer.data(), buffer.size()) == buffer.size() && buffer == signature;
}

namespace
{
auto IsValidUtf8(const std::string &text) -> bool
{
    size_t i = 0;
    const size_t n = text.size();
    while (i < n)
    {
        const auto c = static_cast<uint8_t>(text[i]);
        size_t need = 0;
        if (c < 0x80)
            need = 0;
        else if (c >= 0xC2 && c <= 0xDF)
            need = 1;
        else if (c >= 0xE0 && c <= 0xEF)
            need = 2;
        else if (c >= 0xF0 && c <= 0xF4)
            need = 3;
        else
            return false;
        if (i + need >= n && need > 0)
            return false;
        for (size_t k = 1; k <= need; ++k)
        {
            if ((static_cast<uint8_t>(text[i + k]) & 0xC0) != 0x80)
                return false;
        }
        i += need + 1;
    }
    return true;
}

// Windows-1252 characters 0x80..0x9F as Unicode (undefined ones are kept as is)
constexpr std::array<char16_t, 32> Cp1252 = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};
} // namespace

auto EnsureUtf8(const std::string &text) -> std::string
{
    if (IsValidUtf8(text))
        return text;
    std::u16string wide;
    wide.reserve(text.size());
    for (const char ch : text)
    {
        const auto c = static_cast<uint8_t>(ch);
        wide += (c >= 0x80 && c <= 0x9F) ? Cp1252[c - 0x80] : static_cast<char16_t>(c);
    }
    return Utf16ToUtf8(wide);
}

auto Utf16ToUtf8(const std::u16string &text) -> std::string
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
    {
        uint32_t cp = text[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
        {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (text[i + 1] - 0xDC00);
            ++i;
        }
        // A lone surrogate is not a character: dropped (as the Pascal driver does), never written as invalid UTF-8
        if (cp >= 0xD800 && cp <= 0xDFFF)
            continue;
        if (cp < 0x80)
            out += static_cast<char>(cp);
        else if (cp < 0x800)
        {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}
} // namespace Dud
