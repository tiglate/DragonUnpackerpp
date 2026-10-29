// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Read-only file with a read-ahead cache and the little-endian readers the formats need.
// The format parsers read tables a few bytes at a time (a PAK with 300,000 entries did 900,000
// read() calls), hence the cache; the logical position and the size are kept here.
#pragma once

#include <bit>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Dud
{
static_assert(std::endian::native == std::endian::little, "the readers assume a little-endian host");

// A failure with a DUDI7_* status (what the driver reports to the host)
class Error : public std::runtime_error
{
  public:
    Error(int32_t code, const std::string &message) : std::runtime_error(message), _code(code)
    {
    }
    auto Code() const noexcept -> int32_t
    {
        return _code;
    }

  private:
    int32_t _code;
};

// A buffer that could not be read in full (truncated file)
class ReadError : public std::runtime_error
{
  public:
    ReadError() : std::runtime_error("Unexpected end of file")
    {
    }
};

class Reader
{
  public:
    // A file
    explicit Reader(const std::string &utf8Path);
    // Bytes already in memory (a decompressed directory, ...)
    explicit Reader(std::vector<uint8_t> data);

    auto Size() const noexcept -> int64_t
    {
        return _fileSize;
    }
    // The path given to the constructor (UTF-8)
    auto Path() const noexcept -> const std::string &
    {
        return _path;
    }
    auto Pos() const noexcept -> int64_t
    {
        return _pos;
    }
    void Seek(int64_t position) noexcept
    {
        _pos = position < 0 ? 0 : position;
    }
    void Skip(int64_t count) noexcept
    {
        Seek(_pos + count);
    }

    // Bytes actually read (less than count at the end of the file)
    auto Read(void *buffer, size_t count) -> size_t;
    // All of it or ReadError
    void ReadBuffer(void *buffer, size_t count);

    template <size_t N> void ReadBuffer(char (&buffer)[N])
    {
        ReadBuffer(buffer, N);
    }

    // Little-endian readers: Error(DUDI7_ERR_IO) at the end of the file
    auto U8() -> uint8_t;
    auto U16() -> uint16_t;
    auto U32() -> uint32_t;
    auto I32() -> int32_t;
    auto U64() -> uint64_t;
    auto I64() -> int64_t;
    auto BE32() -> uint32_t;
    auto BE16() -> uint16_t;
    // Null terminated text (at most maxLen characters, Error beyond)
    auto CString(size_t maxLen = 1024) -> std::string;
    // 32 bits length (in bytes) then the characters (cut at the first null)
    auto LString32(size_t maxLen = 1024) -> std::string;
    // 32 bits length (in characters) then UTF-16LE characters, returned as UTF-8
    auto WString32(size_t maxLen = 1024) -> std::string;

  private:
    static constexpr size_t CacheSize = 65536;

    std::string _path;
    bool _inMemory = false;
    std::vector<uint8_t> _memory;
    std::ifstream _file;
    std::vector<char> _cache;
    int64_t _cacheStart = 0;
    size_t _cacheLen = 0;
    int64_t _pos = 0;
    int64_t _fileSize = 0;
};

// Text from a fixed size, possibly not null terminated, buffer
auto ZString(const char *buffer, size_t size) -> std::string;
template <size_t N> auto ZString(const char (&buffer)[N]) -> std::string
{
    return ZString(buffer, N);
}

// Does the file start with these bytes? (signature tests; the position is left after them)
auto StartsWith(Reader &reader, std::string_view signature) -> bool;
// Do the bytes of the buffer equal these ones?
template <size_t N> auto Equals(const char (&buffer)[N], std::string_view text) -> bool
{
    return text.size() <= N && std::string_view(buffer, text.size()) == text;
}

// The contract promises UTF-8 names: valid UTF-8 is kept as is, anything else is taken as
// Windows-1252 (what the games using 8 bits names use) and converted
auto EnsureUtf8(const std::string &text) -> std::string;
auto Utf16ToUtf8(const std::u16string &text) -> std::string;
} // namespace Dud
