// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Requiem: Avenging Angel animation pack (Models/AnimFile.ary + AnimFile.bin; AnimFile.tab maps the animation
// ids of the game to records and is not needed). Either file can be opened, the other is looked for in the same
// directory. The .ARY index is a u32 count, then per record 16 bytes: offset u32, 0, size u32, 0xCDCDCDCD (left
// over memory of the tool that wrote it); the records follow one another and fill the .BIN. The records are
// the animation data as the game loads them (their layout is not known), listed as they are.

#include <cstdio>

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxRecords = 1000000;

class RequiemAnimArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        std::string aryPath;
        std::string binPath;
        if (!Locate(stream.Path(), aryPath, binPath))
            return false;
        std::vector<Record> records;
        return ReadIndex(aryPath, Reader(binPath).Size(), records);
    }

    void Parse() override
    {
        std::string aryPath;
        std::string binPath;
        if (!Locate(_stream->Path(), aryPath, binPath))
            Fail(DUDI7_ERR_NOTFORMAT, "The .ARY/.BIN pair is incomplete");
        _stream = std::make_unique<Reader>(binPath);
        std::vector<Record> records;
        if (!ReadIndex(aryPath, _stream->Size(), records))
            Fail(DUDI7_ERR_NOTFORMAT, "Not a Requiem animation pack");
        _formatId = "Requiem ANIM";
        _dirSeparator = "";
        char name[32];
        for (size_t i = 0; i < records.size(); ++i)
        {
            std::snprintf(name, sizeof name, "anim_%03zu.bin", i);
            Add(name, records[i].Offset, records[i].Size);
        }
    }

  private:
    struct Record
    {
        int64_t Offset;
        int64_t Size;
    };

    static auto Locate(const std::string &path, std::string &ary, std::string &bin) -> bool
    {
        const std::string ext = ToUpper(ExtensionOf(path));
        if (ext == ".ARY")
        {
            ary = path;
            bin = Sibling(path, ".bin");
        }
        else if (ext == ".BIN")
        {
            bin = path;
            ary = Sibling(path, ".ary");
        }
        return !ary.empty() && !bin.empty();
    }

    // The records of the index; false unless they follow one another and end where the .BIN ends
    static auto ReadIndex(const std::string &aryPath, int64_t binSize, std::vector<Record> &records) -> bool
    {
        Reader ary(aryPath);
        if (ary.Size() < 20)
            return false;
        const uint32_t count = ary.U32();
        if (count == 0 || count > MaxRecords || ary.Size() != 4 + static_cast<int64_t>(count) * 16)
            return false;
        int64_t expected = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = ary.U32();
            const uint32_t zero = ary.U32();
            const uint32_t size = ary.U32();
            ary.U32();
            if (offset != expected || zero != 0)
                return false;
            expected += size;
            records.push_back({offset, size});
        }
        return expected == binSize;
    }
};
} // namespace

void RegisterRequiemAnim()
{
    Register<RequiemAnimArchive>("*.ARY;*.BIN", "Requiem: Avenging Angel animations (*.ARY;*.BIN)");
}
} // namespace Dud
