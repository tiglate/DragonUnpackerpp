// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Requiem: Avenging Angel level files (.JAM, and Maps/levels.dat). Header (little endian u32): 0xFEED000A, a size
// (unknown meaning), the unpacked size of the level block, a count, a version (0, 1 or 2), 250, then a mostly
// zero header up to the level block: a u32 packed size followed by a PKWARE DCL stream (codecs/Dcl). The block
// starts after 0x112 bytes for version 0, 0x111E for version 1 and 0x212A for version 2 (0x112 + version * 0x100C) (what the header holds
// there is not known). What follows the block (more than half of the file) is not compressed.
//
// A second DCL stream, with its unpacked size in front, starts at offset (header field at 4) + 12 inside the
// rest: the level data proper (it starts with 0x0BEAF0CC at offset 4, and holds two tables of 256 RGBX colors at
// 0x1F and 0x41F, the second is the palette of the software renderer pictures).
//
// The entries are the header (header.bin), the first unpacked block (level.bin, 16 bits lighting values at least
// at its start), the rest (rest.bin), the second unpacked block (data.bin) and its palette (palette.pal, 768
// bytes of RGB, which the interface uses for the pictures of the same archive). The second block is a memory image of
// the game's objects: its level geometry is also given as level.gltf (codecs/RequiemLevel), with the slots of the
// art archive as textures. The rest of the blocks (the lightmaps of level.bin, the other objects) is not read.

#include "../codecs/Dcl.h"
#include "../codecs/RequiemLevel.h"
#include "PathUtil.h"
#include "Registry.h"
#include "RequiemCommon.h"

namespace Dud
{
namespace
{
constexpr uint32_t Magic = 0xFEED000Au;
constexpr uint32_t MaxLevel = 256u * 1024 * 1024;
constexpr int64_t BlockOffsets[3] = {0x112, 0x111E, 0x212A};

class RequiemJamArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        Layout layout;
        return ReadLayout(stream, layout);
    }

    void Parse() override
    {
        Layout layout;
        if (!ReadLayout(*_stream, layout))
            Fail(DUDI7_ERR_NOTFORMAT, "Not a Requiem level file");
        _formatId = "Requiem JAM";
        _dirSeparator = "";
        _extractInternal = true;
        _layout = layout;
        _levelName = ChangeFileExt(FileNameOf(_stream->Path()), "");
        Add("header.bin", 0, layout.BlockStart);
        Add("level.bin", layout.BlockStart, layout.Unpacked);
        if (layout.RestStart < _stream->Size())
            Add("rest.bin", layout.RestStart, _stream->Size() - layout.RestStart);
        Requiem::Palette palette;
        Requiem::DataBlock data;
        if (Requiem::ReadPalette(*_stream, palette) && Requiem::LocateData(*_stream, data))
        {
            _data = data;
            Add("data.bin", data.Packed, data.Unpacked);
            Add("palette.pal", data.Packed, Requiem::PaletteBytes);
            // the glTF is built here for its size and again when it is extracted (it is not kept)
            std::vector<uint8_t> unpacked;
            if (Unpack(true, unpacked))
            {
                const std::string gltf = LevelGltf(unpacked);
                if (!gltf.empty())
                    Add("level.gltf", data.Packed, static_cast<int64_t>(gltf.size()));
            }
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.Name == "palette.pal")
        {
            Requiem::Palette palette;
            if (!Requiem::ReadPalette(*_stream, palette))
            {
                LastError = "The palette does not unpack";
                return DUDI7_ERR_IO;
            }
            return ExtractBytes(palette.data(), static_cast<int64_t>(palette.size()), sink);
        }
        if (entry.Name != "level.bin" && entry.Name != "data.bin" && entry.Name != "level.gltf")
            return Archive::Extract(index, sink);
        std::vector<uint8_t> out;
        if (!Unpack(entry.Name != "level.bin", out))
        {
            LastError = "The block does not unpack";
            return DUDI7_ERR_IO;
        }
        if (entry.Name == "level.gltf")
        {
            const std::string gltf = LevelGltf(out);
            if (static_cast<int64_t>(gltf.size()) != entry.Size)
            {
                LastError = "The level geometry cannot be read";
                return DUDI7_ERR_IO;
            }
            return ExtractBytes(reinterpret_cast<const uint8_t *>(gltf.data()), entry.Size, sink);
        }
        return ExtractBytes(out.data(), static_cast<int64_t>(out.size()), sink);
    }

  private:
    // The first (level.bin) or the second (data.bin) block unpacked; false if it does not unpack to its size
    auto Unpack(bool second, std::vector<uint8_t> &out) -> bool
    {
        const int64_t start = second ? _data.Packed : _layout.BlockStart + 4;
        const int64_t unpacked = second ? _data.Unpacked : _layout.Unpacked;
        const int64_t length = second ? _stream->Size() - start : _layout.Packed;
        std::vector<uint8_t> packed(static_cast<size_t>(length));
        _stream->Seek(start);
        _stream->ReadBuffer(packed.data(), packed.size());
        return DclExplode(packed.data(), packed.size(), static_cast<size_t>(unpacked), out) && static_cast<int64_t>(out.size()) == unpacked;
    }

    // The level geometry of the second block as glTF (empty if it has none or cannot be read)
    auto LevelGltf(const std::vector<uint8_t> &data) const -> std::string
    {
        RequiemLevel::Level level;
        if (!RequiemLevel::Read(data, level))
            return {};
        return RequiemLevel::MakeGltf(level, _levelName);
    }

    struct Layout
    {
        int64_t BlockStart = 0; // the packed size field
        int64_t Packed = 0;     // bytes of the DCL stream
        int64_t Unpacked = 0;
        int64_t RestStart = 0;
    };

    static auto ReadLayout(Reader &stream, Layout &layout) -> bool
    {
        if (stream.Size() < 0x120)
            return false;
        stream.Seek(0);
        if (stream.U32() != Magic)
            return false;
        stream.U32();
        const uint32_t unpacked = stream.U32();
        stream.U32();
        const uint32_t version = stream.U32();
        if (version > 2 || unpacked == 0 || unpacked > MaxLevel)
            return false;
        const int64_t start = BlockOffsets[version];
        if (start + 8 > stream.Size())
            return false;
        stream.Seek(start);
        const uint32_t packed = stream.U32();
        const uint8_t type = stream.U8();
        const uint8_t dictionary = stream.U8();
        if (type > 1 || dictionary < 4 || dictionary > 6 || packed < 4 || start + 4 + static_cast<int64_t>(packed) > stream.Size())
            return false;
        layout.BlockStart = start;
        layout.Packed = packed;
        layout.Unpacked = unpacked;
        layout.RestStart = start + 4 + packed;
        return true;
    }

    Layout _layout;
    Requiem::DataBlock _data;
    std::string _levelName;
};
} // namespace

void RegisterRequiemJam()
{
    Register<RequiemJamArchive>("*.JAM;*.DAT", "Requiem: Avenging Angel levels (*.JAM;*.DAT)");
}
} // namespace Dud
