// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Common ground of the formats supported by the default driver: the archive base class (entries
// list, raw extraction) and the registry of formats.
//
// To add a format: write a class deriving from Archive in src/formats/ (a static Check and
// Parse), give the file a Register<Name> function that calls Register<T>(extensions, name), and
// list it in FormatList.inc, where the order is the order in which the signatures are tried.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Reader.h"
#include "dudi7.h"

namespace Dud
{
// Where the extracted bytes go (the host's write callback)
class Sink
{
  public:
    Sink(dudi7_write_cb callback, void *context) : _callback(callback), _context(context)
    {
    }
    // False when the host stopped the extraction
    auto Write(const void *data, int64_t length) const -> bool
    {
        return _callback(_context, data, length) == length;
    }

  private:
    dudi7_write_cb _callback;
    void *_context;
};

struct Entry
{
    std::string Name;
    int64_t Offset = 0;
    int64_t Size = 0;
    int64_t DataX = 0;
    int64_t DataY = 0;
};

class Archive
{
  public:
    virtual ~Archive() = default;
    Archive() = default;
    Archive(const Archive &) = delete;
    auto operator=(const Archive &) -> Archive & = delete;

    // True for formats without a reliable signature: they are only tried on files whose
    // extension is one of the registered ones (hidden by the formats that need it)
    static auto RequiresExtension() -> bool
    {
        return false;
    }

    // Read the entries (_stream is open). Throws Error on failure.
    virtual void Parse() = 0;
    // The data of an entry; a DUDI7_* status, LastError describes failures
    virtual auto Extract(int64_t index, const Sink &sink) -> int32_t;

    void SetStream(std::unique_ptr<Reader> stream)
    {
        _stream = std::move(stream);
    }

    auto Count() const noexcept -> int64_t
    {
        return static_cast<int64_t>(_entries.size());
    }
    auto Entries() const noexcept -> const std::vector<Entry> &
    {
        return _entries;
    }
    auto FormatId() const noexcept -> const std::string &
    {
        return _formatId;
    }
    auto DirSeparator() const noexcept -> const std::string &
    {
        return _dirSeparator;
    }
    auto ExtractInternal() const noexcept -> bool
    {
        return _extractInternal;
    }

    std::string LastError;

    // Inflate unpackedSize bytes of a zlib (or raw deflate) stream that starts at offset of the
    // reader, for the formats with a compressed directory. Error(DUDI7_ERR_NOTFORMAT) on failure.
    static auto Unpack(Reader &source, int64_t offset, int64_t unpackedSize, bool raw = false)
        -> std::vector<uint8_t>;

  protected:
    // Record a new entry (offset/size describe the data in the file)
    void Add(const std::string &name, int64_t offset, int64_t size, int64_t dataX = 0, int64_t dataY = 0);
    [[noreturn]] static void Fail(int32_t code, const std::string &message);
    // Fail unless count entries of entrySize bytes starting at offset fit in the file
    void CheckTable(int64_t offset, int64_t count, int64_t entrySize) const;
    // Called by the default Extract on every chunk read, to decode data that is stored
    // encrypted (set _extractInternal): pos is the position of the chunk in the entry
    virtual void DecodeChunk(int64_t index, uint8_t *buffer, int64_t length, int64_t pos);

    // Extract helpers for compressed entries: a zlib stream of packedSize bytes at offset giving
    // unpackedSize bytes
    auto ExtractZlib(int64_t offset, int64_t packedSize, int64_t unpackedSize, const Sink &sink) -> int32_t;
    // the same for a raw deflate stream (no zlib header)
    auto ExtractDeflate(int64_t offset, int64_t packedSize, int64_t unpackedSize, const Sink &sink) -> int32_t;
    // a block of bytes already in memory
    auto ExtractBytes(const void *data, int64_t length, const Sink &sink) -> int32_t;

    std::unique_ptr<Reader> _stream;
    std::vector<Entry> _entries;
    std::string _formatId;
    std::string _dirSeparator;
    // Set by descendants whose data cannot be copied as is from the file
    bool _extractInternal = false;

  private:
    auto Inflate(int64_t offset, int64_t packedSize, int64_t unpackedSize, bool raw, const Sink &sink) -> int32_t;
};
} // namespace Dud
