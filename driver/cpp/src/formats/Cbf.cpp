// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Vietcong .CBF support ("BIGF" 0x00 "ZBL", and the crypted form "BIGF" 0x01 "ZBL" of Vietcong and Vietcong 2),
// ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (64 bytes): the 8 bytes of the id, size of the file (64 bits), number of entries, offset and size of the
// directory (64 bits each), 7 unknown integers. Entry (40 bytes): offset (64 bits), unknown, file time (64 bits),
// size (64 bits), packed size, compression type (1 = LZW of the game), unknown (0x24), then the name (null
// terminated). In the crypted form each directory record has a 16 bits size in front and is encrypted; the data of
// the files that are not compressed is encrypted too. Compressed files are made of chunks "[..]", packed size,
// unpacked size, and the LZW data (see codecs/Unlzw).
// The decryption routines are adapted from cbfext.c v0.2.1 by Luigi Auriemma (GPL v2 or later).

#include <algorithm>
#include <cstring>

#include "Registry.h"
#include "codecs/Unlzw.h"

namespace Dud
{
namespace
{
constexpr size_t HeaderSize = 64;
constexpr size_t EntrySize = 40;

// decrypts a directory record
void HeadDecrypt(std::vector<uint8_t> &chunk)
{
    static const uint8_t key[16] = {0x32, 0xf3, 0x1e, 0x06, 0x45, 0x70, 0x32, 0xaa,
                                    0x55, 0x3f, 0xf1, 0xde, 0xa3, 0x44, 0x21, 0xb4};
    int e2 = static_cast<int>(chunk.size());
    for (uint8_t &byte : chunk)
    {
        const int t = byte;
        byte = static_cast<uint8_t>(byte ^ key[e2 & 15]);
        e2 = t;
    }
}

// decrypts the data of a file that is not compressed
void FileDecrypt(uint8_t *data, size_t length, int64_t totalSize)
{
    const auto t1 = static_cast<uint8_t>(totalSize);
    const auto t2 = static_cast<uint8_t>(90 - t1);
    for (size_t i = 0; i < length; ++i)
        data[i] = static_cast<uint8_t>(static_cast<uint8_t>(data[i] - t2) ^ t1);
}

template <bool Crypted> class CbfArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[8];
        if (stream.Read(id, 8) != 8 || std::memcmp(id, "BIGF", 4) != 0 || id[4] != (Crypted ? 1 : 0) ||
            std::memcmp(id + 5, "ZBL", 3) != 0)
            return false;
        return stream.I64() == stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(8);
        const int64_t fileSize = _stream->I64();
        const int32_t dirNum = _stream->I32();
        const int64_t dirOffset = _stream->I64();
        if (fileSize != _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid file size");
        if (dirNum < 0 || dirOffset < static_cast<int64_t>(HeaderSize) || dirOffset > _stream->Size() ||
            dirNum > (_stream->Size() - dirOffset) / (Crypted ? 3 : 41))
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory");
        _formatId = Crypted ? "CBF1" : "CBF0";
        _dirSeparator = "\\";
        _extractInternal = true;
        _stream->Seek(dirOffset);
        for (int32_t x = 1; x <= dirNum; ++x)
        {
            std::vector<uint8_t> record;
            if (Crypted)
            {
                const uint16_t size = _stream->U16();
                record.resize(size);
                if (size > 0)
                    _stream->ReadBuffer(record.data(), size);
                HeadDecrypt(record);
            }
            else
            {
                record.resize(EntrySize);
                _stream->ReadBuffer(record.data(), EntrySize);
            }
            if (record.size() < EntrySize)
                Fail(DUDI7_ERR_NOTFORMAT, "A directory record is too short");
            std::string name;
            if (Crypted)
            {
                size_t end = EntrySize;
                while (end < record.size() && record[end] != 0)
                    ++end;
                name.assign(reinterpret_cast<const char *>(&record[EntrySize]), end - EntrySize);
            }
            else
            {
                char c;
                for (;;)
                {
                    _stream->ReadBuffer(&c, 1);
                    if (c == '\0')
                        break;
                    name += c;
                    if (name.size() > 4096)
                        Fail(DUDI7_ERR_NOTFORMAT, "Invalid name");
                }
            }
            int64_t offset, size;
            int32_t compSize, compType;
            std::memcpy(&offset, record.data(), 8);
            std::memcpy(&size, record.data() + 20, 8);
            std::memcpy(&compSize, record.data() + 28, 4);
            std::memcpy(&compType, record.data() + 32, 4);
            const int64_t stored = compType == 1 ? compSize : size;
            if (size < 0 || offset < 0 || (size > 0 && offset + stored > _stream->Size()) || stored < 0)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, size, compSize, compType);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataY == 1)
            return Decompress(entry, sink);
        if (!Crypted)
            return Archive::Extract(index, sink);
        // encrypted, by pieces
        _stream->Seek(entry.Offset);
        std::vector<uint8_t> buffer(65536);
        int64_t left = entry.Size;
        while (left > 0)
        {
            const size_t n = static_cast<size_t>(std::min<int64_t>(left, buffer.size()));
            _stream->ReadBuffer(buffer.data(), n);
            FileDecrypt(buffer.data(), n, entry.Size);
            const int32_t result = ExtractBytes(buffer.data(), static_cast<int64_t>(n), sink);
            if (result != DUDI7_OK)
                return result;
            left -= static_cast<int64_t>(n);
        }
        return DUDI7_OK;
    }

  private:
    auto Decompress(const Entry &entry, const Sink &sink) -> int32_t
    {
        const int64_t end = entry.Offset + entry.DataX;
        int64_t outSize = 0;
        _stream->Seek(entry.Offset);
        while (outSize != entry.Size && _stream->Pos() < end)
        {
            char magic[4];
            _stream->ReadBuffer(magic, 4);
            const int32_t csize = _stream->I32();
            const int32_t osize = _stream->I32();
            // (the magic is not checked: the game's own routine does not either)
            if (csize < 0 || osize < 0 || _stream->Pos() + csize > _stream->Size() || osize > 1024 * 1024 * 1024)
            {
                LastError = "Invalid compressed chunk";
                return DUDI7_ERR_IO;
            }
            std::vector<uint8_t> packed(static_cast<size_t>(csize));
            if (csize > 0)
                _stream->ReadBuffer(packed.data(), packed.size());
            std::vector<uint8_t> unpacked(static_cast<size_t>(osize), 0);
            UnlzwDecode(packed.data(), packed.size(), unpacked.data(), unpacked.size());
            const int32_t result = ExtractBytes(unpacked.data(), osize, sink);
            if (result != DUDI7_OK)
                return result;
            outSize += osize;
        }
        return DUDI7_OK;
    }
};
} // namespace

void RegisterCbf()
{
    Register<CbfArchive<false>>("*.CBF", "Vietcong Archive (*.CBF)");
}

void RegisterCbfCrypted()
{
    Register<CbfArchive<true>>("*.CBF", "Vietcong Crypted Archive (*.CBF)");
}
} // namespace Dud
