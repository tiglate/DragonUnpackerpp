// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Elder Scrolls 4: Oblivion .BSA support (version 103), ported from
// drv_default.dpr of Dragon UnPACKer 5.
// Header (36 bytes): "BSA" + null, version (103), offset of the folder records
// (36), archive flags (1: folder names, 2: file names, 4: compressed by
// default), folder count, file count, total length of the folder names, total
// length of the file names, file flags.
// Folder records (16 bytes): hash, number of files, offset (+ the total length of
// the file names). Each folder block: its name (length byte + text + null) and the
// file records (16 bytes: hash, size, offset). The file names follow all the
// folders, in the same order. A file is compressed when the default flag differs
// from the bit $40000000 of its size: its data is the original size (32 bits)
// then a zlib stream (the entry is the original size, DataX = 1, DataY = packed size).

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t Compressed = 0x40000000;
constexpr uint32_t SizeMask = 0x3FFFFFFF;
constexpr int64_t MaxUnpacked = 512 * 1024 * 1024;

class BsaArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, std::string_view("BSA\0", 4)) && stream.Size() >= 36 && stream.U32() == 103;
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t folderOffset = _stream->U32();
        const uint32_t flags = _stream->U32();
        const uint32_t folderCount = _stream->U32();
        const uint32_t fileCount = _stream->U32();
        const uint32_t folderNames = _stream->U32();
        const uint32_t fileNames = _stream->U32();
        if ((flags & 3) != 3)
            Fail(DUDI7_ERR_UNSUPPORTED, "BSA archives without folder or file names are not supported");
        CheckTable(folderOffset, folderCount, 16);
        const int64_t offsetToNames = static_cast<int64_t>(folderOffset) + static_cast<int64_t>(folderCount) * 17 +
                                      static_cast<int64_t>(fileCount) * 16 + folderNames;
        if (offsetToNames + fileNames > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "The file names are outside of the archive");

        std::vector<uint8_t> block(fileNames);
        _stream->Seek(offsetToNames);
        if (fileNames > 0)
            _stream->ReadBuffer(block.data(), block.size());
        Reader nameReader(std::move(block));
        std::vector<std::string> names;
        for (uint32_t i = 0; i < fileCount; ++i)
            names.push_back(nameReader.CString());

        const bool defaultCompressed = (flags & 4) == 4;
        _formatId = "BSA";
        _dirSeparator = "\\";
        _extractInternal = true;
        size_t n = 0;
        for (uint32_t i = 0; i < folderCount; ++i)
        {
            _stream->Seek(static_cast<int64_t>(folderOffset) + static_cast<int64_t>(i) * 16 + 8);
            const uint32_t filesInFolder = _stream->U32();
            // The offset of the folder block includes the total length of the file names
            const int64_t blockOffset = static_cast<int64_t>(_stream->U32()) - fileNames;
            if (blockOffset < 0 || blockOffset >= _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid folder offset");
            _stream->Seek(blockOffset);
            const uint8_t dirLength = _stream->U8();
            std::string dir(dirLength, '\0');
            if (dirLength > 0)
                _stream->ReadBuffer(dir.data(), dirLength);
            dir = ZString(dir.data(), dirLength);
            const int64_t recordBase = blockOffset + 1 + dirLength;
            // The Pascal loop counter is an Integer: a count of 2^31 or more makes the bound negative and the
            // folder is silently empty. Kept, so that damaged files are read as before.
            const auto lastFile = static_cast<int32_t>(static_cast<int64_t>(filesInFolder) - 1);
            for (int64_t j = 0; j <= lastFile; ++j)
            {
                if (n >= names.size())
                    Fail(DUDI7_ERR_NOTFORMAT, "More files than names");
                // file record: hash (8), size, offset
                _stream->Seek(recordBase + static_cast<int64_t>(j) * 16 + 8);
                const uint32_t rawSize = _stream->U32();
                const uint32_t dataOffset = _stream->U32();
                const bool fileCompressed = (rawSize & Compressed) != 0;
                const uint32_t size = rawSize & SizeMask;
                if (static_cast<int64_t>(dataOffset) + size > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                if (defaultCompressed != fileCompressed)
                {
                    // Compressed: the original size comes first
                    if (size < 4)
                        Fail(DUDI7_ERR_NOTFORMAT, "Invalid compressed file");
                    _stream->Seek(dataOffset);
                    const uint32_t originalSize = _stream->U32();
                    Add(dir + "\\" + names[n], static_cast<int64_t>(dataOffset) + 4, originalSize, 1, size - 4);
                }
                else
                {
                    Add(dir + "\\" + names[n], dataOffset, size, 0, 0);
                }
                ++n;
            }
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == 0)
            return Archive::Extract(index, sink);
        if (entry.Size > MaxUnpacked || entry.Offset + entry.DataY > _stream->Size())
        {
            LastError = "Invalid compressed entry";
            return DUDI7_ERR_IO;
        }
        std::vector<uint8_t> data;
        try
        {
            data = Unpack(*_stream, entry.Offset, entry.Size);
        }
        catch (const Error &e)
        {
            LastError = std::string("Cannot unpack the file: ") + e.what();
            return DUDI7_ERR_IO;
        }
        return ExtractBytes(data.data(), static_cast<int64_t>(data.size()), sink);
    }
};
} // namespace

void RegisterBsa()
{
    Register<BsaArchive>("*.BSA", "The Elder Scrolls 4: Oblivion (*.BSA)");
}
} // namespace Dud
