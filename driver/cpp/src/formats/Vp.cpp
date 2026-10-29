// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Volition .VP support: Conflict Freespace, Freespace 2
// (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (16 bytes): "VPVP", version (2), directory offset, number of
// directory items. Items are 44 bytes (offset, size, name[32], timestamp):
// a size of 0 is a directory ("..": go back to the parent), others are files.

#include "Registry.h"

namespace Dud
{
namespace
{
class VpArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "VPVP") && stream.U32() == 2;
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t dirOffset = _stream->U32();
        const uint32_t count = _stream->U32();
        CheckTable(dirOffset, count, 44);

        _formatId = "VP";
        _dirSeparator = "\\";
        _stream->Seek(dirOffset);
        std::string currentDir;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            char name[32];
            _stream->ReadBuffer(name);
            _stream->U32();
            const std::string display = ZString(name);
            if (size != 0)
            {
                Add(currentDir + display, offset, size);
            }
            else if (display == "..")
            {
                // Remove the last directory
                if (!currentDir.empty())
                    currentDir.pop_back();
                const auto slash = currentDir.find_last_of('\\');
                currentDir = slash == std::string::npos ? "" : currentDir.substr(0, slash + 1);
            }
            else
            {
                currentDir += display + "\\";
            }
        }
    }
};
} // namespace

void RegisterVp()
{
    Register<VpArchive>("*.VP", "Conflict Freespace (*.VP)|Freespace 2 (*.VP)");
}
} // namespace Dud
