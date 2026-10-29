// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Grand Theft Auto 3 .IMG/.DIR support (ported from drv_default.dpr of Dragon
// UnPACKer 5). Two files: the .DIR index (32 bytes per file: first block, number
// of blocks, name[24]) and the .IMG data (blocks of 2048 bytes). Either file
// can be opened, the other is looked for in the same directory. Sizes are
// the sizes in blocks: files are padded to a multiple of 2048 bytes.

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t Block = 2048;

class Gta3ImgArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        int64_t dirSize = 0;
        int64_t imgSize = 0;
        if (ToUpper(ExtensionOf(stream.Path())) == ".DIR")
        {
            const std::string other = Sibling(stream.Path(), ".img");
            dirSize = stream.Size();
            if (other.empty())
                return false;
            imgSize = Reader(other).Size();
        }
        else
        {
            const std::string other = Sibling(stream.Path(), ".dir");
            imgSize = stream.Size();
            if (other.empty())
                return false;
            dirSize = Reader(other).Size();
        }
        return dirSize > 0 && dirSize % 32 == 0 && imgSize > 0 && imgSize % Block == 0;
    }

    void Parse() override
    {
        std::string dirName;
        if (ToUpper(ExtensionOf(_stream->Path())) == ".DIR")
        {
            dirName = _stream->Path();
            // The archive data is in the .IMG file
            _stream = std::make_unique<Reader>(Sibling(dirName, ".img"));
        }
        else
        {
            dirName = Sibling(_stream->Path(), ".dir");
        }
        Reader dir(dirName);
        const int64_t count = dir.Size() / 32;
        _formatId = "DIR/IMG";
        _dirSeparator = "";
        for (int64_t i = 0; i < count; ++i)
        {
            const uint32_t start = dir.U32();
            const uint32_t blocks = dir.U32();
            char name[24];
            dir.ReadBuffer(name);
            if ((static_cast<int64_t>(start) + blocks) * Block > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the .IMG");
            Add(ZString(name), static_cast<int64_t>(start) * Block, static_cast<int64_t>(blocks) * Block);
        }
    }
};
} // namespace

void RegisterGta3Img()
{
    Register<Gta3ImgArchive>("*.IMG;*.DIR", "GTA3/Grand Theft Auto 3 (*.IMG;*.DIR)");
}
} // namespace Dud
