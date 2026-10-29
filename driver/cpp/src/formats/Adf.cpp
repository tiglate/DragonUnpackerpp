// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// GTA: Vice City .ADF support, ported from drv_default.dpr of Dragon UnPACKer 5.
// An .ADF file is an MP3 file with every byte xored with $22. There is no directory: the
// archive holds one entry, named after the file with the extension ".mp3". The first
// bytes are checked: an MP3 frame header ($FF, then $E0 as the top bits) once xored.

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
class AdfArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint8_t b[2];
        return stream.Read(b, 2) == 2 && (b[0] ^ 0x22) == 0xFF && ((b[1] ^ 0x22) & 0xE0) == 0xE0;
    }

    void Parse() override
    {
        _formatId = "ADF";
        _dirSeparator = "";
        _extractInternal = true;
        Add(ChangeFileExt(FileNameOf(_stream->Path()), ".mp3"), 0, _stream->Size());
    }

  protected:
    void DecodeChunk(int64_t, uint8_t *buffer, int64_t length, int64_t) override
    {
        for (int64_t i = 0; i < length; ++i)
            buffer[i] ^= 0x22;
    }
};
} // namespace

void RegisterAdf()
{
    Register<AdfArchive>("*.ADF", "GTA: Vice City Audio (*.ADF)");
}
} // namespace Dud
