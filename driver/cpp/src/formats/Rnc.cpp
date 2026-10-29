// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// RNC ProPack compressed files, used as is by Silver (*.PAK): the whole file
// is one RNC method 1 stream. It is presented as a single entry whose data
// is the unpacked stream (see codecs/Rnc.cpp for the decompression).

#include "PathUtil.h"
#include "Registry.h"
#include "codecs/Rnc.h"

namespace Dud
{
namespace
{
class RncArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint8_t buffer[RncHeaderSize];
        RncHeader header;
        return stream.Read(buffer, sizeof buffer) == sizeof buffer && RncReadHeader(buffer, sizeof buffer, header);
    }

    void Parse() override
    {
        _stream->Seek(0);
        uint8_t buffer[RncHeaderSize];
        _stream->ReadBuffer(buffer, sizeof buffer);
        if (!RncReadHeader(buffer, sizeof buffer, _header))
            Fail(DUDI7_ERR_NOTFORMAT, "Not an RNC file");
        if (static_cast<int64_t>(_header.PackedSize) + static_cast<int64_t>(RncHeaderSize) > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "RNC packed data is truncated");
        if (_header.UnpackedSize > RncMaxUnpackedSize)
            Fail(DUDI7_ERR_UNSUPPORTED, "RNC unpacked size is too large");

        _formatId = "RNC" + std::to_string(_header.Method);
        _dirSeparator = "";
        _extractInternal = true;
        // Offset/Size describe the packed data; the entry size is the unpacked one
        Add(ChangeFileExt(FileNameOf(_stream->Path()), "") + ".bin", RncHeaderSize, _header.UnpackedSize,
            _header.Method, _header.PackedSize);
    }

    auto Extract(int64_t, const Sink &sink) -> int32_t override
    {
        std::vector<uint8_t> unpacked;
        try
        {
            std::vector<uint8_t> packed(static_cast<size_t>(_header.PackedSize) + RncHeaderSize);
            _stream->Seek(0);
            _stream->ReadBuffer(packed.data(), packed.size());
            unpacked = RncUnpack(packed, _header);
        }
        catch (const RncError &e)
        {
            LastError = e.what();
            return DUDI7_ERR_IO;
        }
        return ExtractBytes(unpacked.data(), static_cast<int64_t>(unpacked.size()), sink);
    }

  private:
    RncHeader _header;
};
} // namespace

void RegisterRnc()
{
    Register<RncArchive>("*.PAK;*.RNC", "Silver (*.PAK)|RNC ProPack compressed file (*.RNC)");
}
} // namespace Dud
