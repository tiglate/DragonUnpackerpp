// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Sims .FAR support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (16 bytes): "FAR!byAZ", version, directory offset. The directory
// starts with the number of files, then for each: size, size again, offset,
// name length (32 bits) and the name.

#include "Registry.h"

namespace Dud
{
namespace
{
class FarArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "FAR!byAZ");
    }

    void Parse() override
    {
        _stream->Seek(12);
        const uint32_t dirOffset = _stream->U32();
        CheckTable(dirOffset, 1, 4);
        _stream->Seek(dirOffset);
        const uint32_t count = _stream->U32();
        // At least 16 bytes per entry
        CheckTable(static_cast<int64_t>(dirOffset) + 4, count, 16);

        _formatId = "FAR";
        _dirSeparator = "\\";
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t size = _stream->U32();
            _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t nameLength = _stream->U32();
            if (nameLength > 1024)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid file name length");
            std::string name(nameLength, '\0');
            if (nameLength > 0)
                _stream->ReadBuffer(name.data(), nameLength);
            Add(ZString(name.data(), nameLength), offset, size);
        }
    }
};
} // namespace

void RegisterFar()
{
    Register<FarArchive>("*.FAR", "The Sims (*.FAR)");
}
} // namespace Dud
