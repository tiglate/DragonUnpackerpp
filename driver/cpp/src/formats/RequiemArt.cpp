// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Requiem: Avenging Angel (Cyclone Studios, 1999) texture archives, found by reading the game's own loader
// (3DFX.exe, ArtManager). Two files: the .DAT index (one u32 offset per texture slot, 0xFFFFFFFF or 0 for an empty
// slot) and the .ART data. Either file can be opened, the other is looked for in the same directory. The game
// ships three sets with the same 2686 textures: Art/SW (software renderer), Art/HW (3dfx) and Art/D3D.
//
// A record is: unpacked size u32, 0 u32, packed size u32 (counts itself, so the next record starts at record +
// 8 + packed size), then a PKWARE DCL stream (codecs/Dcl). The unpacked texture is an 18 byte header (width u16,
// height u16, 9 zero bytes, number of mip levels, bytes per pixel (1: palette index, 2: index and alpha),
// 3 bytes of renderer flags), the pixels and a trailer that depends on the set:
//   D3D  a 256 entry BGRA palette in front of the pixels (1024 bytes)
//   SW   no palette in the file: the pixels (all the mip levels one after the other) are indices into the
//        palette of the levels (Maps/*.jam, see RequiemCommon.h), the same for every level but 4 of them
//   HW   a 3dfx NCC table (Glide's GuNccTable: 16 Y bytes, 4 x 3 signed 9 bits I, 4 x 3 Q, then the same
//        packed in 12 u32 words: 112 bytes) after the pixels; each pixel byte is YYYYIIQQ: color =
//        Y[yyyy] + I[ii] + Q[qq] per channel, clamped
// With 2 bytes per pixel the second byte is the alpha.
//
// Entries are shown as TGA pictures (first mip level only): 32 bits with the alpha channel for D3D and HW, for
// SW the palette of the neighbouring Maps folder when there is one (colour mapped TGA, so that the interface
// can swap the map; grays otherwise). A record that does not fit this layout is listed as it is (.tex).

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <optional>

#include "../codecs/Dcl.h"
#include "PathUtil.h"
#include "Registry.h"
#include "RequiemCommon.h"

namespace Dud
{
namespace
{
constexpr size_t TexHeader = 18;
constexpr size_t PaletteBytes = 1024;
constexpr uint32_t MaxUnpacked = 16u * 1024 * 1024;

struct Slot
{
    uint32_t Offset;
    uint32_t Unpacked;
    uint32_t Packed;
    uint32_t Index; // slot number in the .DAT
};

void Put16(std::vector<uint8_t> &v, unsigned value)
{
    v.push_back(static_cast<uint8_t>(value));
    v.push_back(static_cast<uint8_t>(value >> 8));
}

// The NCC (YIQ) table at the end of the HW textures
constexpr size_t NccBytes = 112;

struct Ncc
{
    uint8_t Y[16];
    int I[4][3];
    int Q[4][3];

    explicit Ncc(const uint8_t *t)
    {
        std::memcpy(Y, t, 16);
        const auto s9 = [](const uint8_t *p) {
            const int v = (p[0] | p[1] << 8) & 0x1FF;
            return v & 0x100 ? v - 512 : v;
        };
        for (int i = 0; i < 4; ++i)
            for (int c = 0; c < 3; ++c)
            {
                I[i][c] = s9(t + 16 + (i * 3 + c) * 2);
                Q[i][c] = s9(t + 40 + (i * 3 + c) * 2);
            }
    }

    // BGR
    void Color(uint8_t pixel, uint8_t bgr[3]) const
    {
        const int y = Y[pixel >> 4];
        for (int c = 0; c < 3; ++c)
            bgr[2 - c] = static_cast<uint8_t>(std::clamp(y + I[(pixel >> 2) & 3][c] + Q[pixel & 3][c], 0, 255));
    }
};

// The TGA for an unpacked texture; empty if the layout is not the known one. needsPalette is set when the
// texture has none of its own: palette (RGB, may be null) then gives the colors
auto MakeTga(const std::vector<uint8_t> &tex, const Requiem::Palette *palette, bool &needsPalette)
    -> std::vector<uint8_t>
{
    needsPalette = false;
    if (tex.size() < TexHeader)
        return {};
    const size_t width = tex[0] | tex[1] << 8;
    const size_t height = tex[2] | tex[3] << 8;
    const size_t levels = tex[13];
    const size_t bpp = tex[14];
    if (width == 0 || height == 0 || width > 4096 || height > 4096 || levels == 0 || levels > 16 || bpp < 1 || bpp > 2)
        return {};
    size_t pixels = 0;
    for (size_t k = 0; k < levels; ++k)
        pixels += std::max<size_t>(1, width >> k) * std::max<size_t>(1, height >> k);
    pixels *= bpp;
    if (TexHeader + pixels > tex.size())
        return {};
    const size_t rest = tex.size() - TexHeader - pixels;
    const bool embedded = rest == PaletteBytes;
    const bool ncc = rest == NccBytes;
    const uint8_t *data = tex.data() + TexHeader + (embedded ? PaletteBytes : 0);
    needsPalette = !embedded && !ncc;
    const bool mapped = needsPalette && palette != nullptr && bpp == 1;
    const bool gray = needsPalette && palette == nullptr && bpp == 1;

    std::vector<uint8_t> tga;
    const bool colorMapped = mapped || gray;
    tga.push_back(0);
    tga.push_back(colorMapped ? 1 : 0);
    tga.push_back(colorMapped ? 1 : 2);
    Put16(tga, 0);
    Put16(tga, colorMapped ? 256 : 0);
    tga.push_back(colorMapped ? 24 : 0);
    Put16(tga, 0);
    Put16(tga, 0);
    Put16(tga, static_cast<unsigned>(width));
    Put16(tga, static_cast<unsigned>(height));
    tga.push_back(colorMapped ? 8 : 32);
    tga.push_back(colorMapped ? 0x00 : 0x08); // bottom-left origin: the rows are stored bottom first, 8 alpha bits
    if (colorMapped)
    {
        for (int i = 0; i < 256; ++i)
            for (int c = 2; c >= 0; --c)
                tga.push_back(mapped ? (*palette)[static_cast<size_t>(i) * 3 + static_cast<size_t>(c)]
                                     : static_cast<uint8_t>(i));
        tga.insert(tga.end(), data, data + width * height);
        return tga;
    }
    static const uint8_t NoTable[NccBytes] = {};
    const Ncc table(ncc ? data + pixels : NoTable);
    tga.reserve(tga.size() + width * height * 4);
    for (size_t i = 0; i < width * height; ++i)
    {
        const uint8_t index = data[i * bpp];
        const uint8_t alpha = bpp == 2 ? data[i * bpp + 1] : 255;
        uint8_t bgr[3];
        if (embedded)
            std::memcpy(bgr, tex.data() + TexHeader + static_cast<size_t>(index) * 4, 3);
        else if (ncc)
            table.Color(index, bgr);
        else if (palette != nullptr)
        {
            for (size_t c = 0; c < 3; ++c)
                bgr[c] = (*palette)[static_cast<size_t>(index) * 3 + 2 - c];
        }
        else
            bgr[0] = bgr[1] = bgr[2] = index;
        tga.insert(tga.end(), {bgr[0], bgr[1], bgr[2], alpha});
    }
    return tga;
}

class RequiemArtArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        std::string datPath;
        std::string artPath;
        if (!Locate(stream.Path(), datPath, artPath))
            return false;
        std::vector<Slot> slots;
        return ReadSlots(datPath, artPath, slots);
    }

    void Parse() override
    {
        std::string datPath;
        std::string artPath;
        if (!Locate(_stream->Path(), datPath, artPath))
            Fail(DUDI7_ERR_NOTFORMAT, "The .DAT/.ART pair is incomplete");
        std::vector<Slot> slots;
        if (!ReadSlots(datPath, artPath, slots))
            Fail(DUDI7_ERR_NOTFORMAT, "Not a Requiem texture archive");
        _stream = std::make_unique<Reader>(artPath);
        _formatId = "Requiem ART";
        _dirSeparator = "";
        _extractInternal = true;
        char name[32];
        _slots = std::move(slots);
        for (const Slot &slot : _slots)
        {
            const std::vector<uint8_t> tex = Unpack(*_stream, slot);
            if (tex.empty())
                Fail(DUDI7_ERR_NOTFORMAT, "A texture does not unpack");
            const std::vector<uint8_t> tga = Render(tex);
            std::snprintf(name, sizeof name, "tex_%04u.%s", slot.Index, tga.empty() ? "tex" : "tga");
            Add(name, slot.Offset, static_cast<int64_t>(tga.empty() ? tex.size() : tga.size()));
            _rawKinds.push_back(tga.empty());
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const auto i = static_cast<size_t>(index);
        std::vector<uint8_t> tex = Unpack(*_stream, _slots[i]);
        if (tex.empty())
        {
            LastError = "The texture does not unpack";
            return DUDI7_ERR_IO;
        }
        if (_rawKinds[i])
            return ExtractBytes(tex.data(), static_cast<int64_t>(tex.size()), sink);
        const std::vector<uint8_t> tga = Render(tex);
        return ExtractBytes(tga.data(), static_cast<int64_t>(tga.size()), sink);
    }

  private:
    static auto Locate(const std::string &path, std::string &dat, std::string &art) -> bool
    {
        const std::string ext = ToUpper(ExtensionOf(path));
        if (ext == ".DAT")
        {
            dat = path;
            art = Sibling(path, ".art");
        }
        else if (ext == ".ART")
        {
            art = path;
            dat = Sibling(path, ".dat");
        }
        return !dat.empty() && !art.empty();
    }

    // The valid slots of the index, checked against the data file (the records follow one another)
    static auto ReadSlots(const std::string &datPath, const std::string &artPath, std::vector<Slot> &slots) -> bool
    {
        Reader dat(datPath);
        Reader art(artPath);
        const int64_t datSize = dat.Size();
        if (datSize < 8 || datSize % 4 != 0 || datSize > 4 * 1024 * 1024 || art.Size() < 12)
            return false;
        const auto count = static_cast<uint32_t>(datSize / 4);
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = dat.U32();
            // Empty slots are 0xFFFFFFFF, or 0 after the first (the game shows the first texture there)
            if (offset == 0xFFFFFFFFu || (offset == 0 && i != 0))
                continue;
            if (static_cast<int64_t>(offset) + 12 > art.Size())
                return false;
            const int64_t here = dat.Pos();
            art.Seek(offset);
            const uint32_t unpacked = art.U32();
            const uint32_t zero = art.U32();
            const uint32_t packed = art.U32();
            if (zero != 0 || unpacked < TexHeader || unpacked > MaxUnpacked || packed < 8 ||
                static_cast<int64_t>(offset) + 8 + packed > art.Size())
                return false;
            dat.Seek(here);
            slots.push_back({offset, unpacked, packed, i});
        }
        // The records do not overlap (the slots are not in the order of the file)
        std::vector<Slot> sorted = slots;
        std::sort(sorted.begin(), sorted.end(), [](const Slot &a, const Slot &b) { return a.Offset < b.Offset; });
        for (size_t i = 1; i < sorted.size(); ++i)
            if (sorted[i].Offset != sorted[i - 1].Offset &&
                static_cast<uint64_t>(sorted[i - 1].Offset) + 8 + sorted[i - 1].Packed > sorted[i].Offset)
                return false;
        return !slots.empty();
    }

    // The picture of a texture; the palette of the levels is looked for the first time a texture needs it
    auto Render(const std::vector<uint8_t> &tex) -> std::vector<uint8_t>
    {
        bool needs = false;
        std::vector<uint8_t> tga = MakeTga(tex, _palette ? &*_palette : nullptr, needs);
        if (needs && !_paletteSearched)
        {
            _paletteSearched = true;
            _palette = FindLevelPalette(_stream->Path());
            tga = MakeTga(tex, _palette ? &*_palette : nullptr, needs);
        }
        return tga;
    }

    // The palette that the levels of the game give to the software renderer pictures: the one most of the
    // Maps/*.jam files have (a few levels have another one). The game folder is the one that has Art and Maps.
    static auto FindLevelPalette(const std::string &artPath) -> std::optional<Requiem::Palette>
    {
        namespace fs = std::filesystem;
        std::error_code error;
        const fs::path art = fs::absolute(fs::path(std::u8string(artPath.begin(), artPath.end())), error);
        for (fs::path dir = art.parent_path(); dir.has_parent_path() && dir != dir.parent_path(); dir = dir.parent_path())
        {
            for (const char *name : {"Maps", "maps", "MAPS"})
            {
                const fs::path maps = dir / name;
                if (!fs::is_directory(maps, error))
                    continue;
                std::vector<fs::path> files;
                for (const auto &item : fs::directory_iterator(maps, error))
                    if (ToLower(Utf8Of(item.path().extension())) == ".jam")
                        files.push_back(item.path());
                std::sort(files.begin(), files.end());
                std::map<Requiem::Palette, int> votes;
                int found = 0;
                for (const fs::path &file : files)
                {
                    try
                    {
                        Reader jam(Utf8Of(file));
                        Requiem::Palette palette;
                        if (Requiem::ReadPalette(jam, palette))
                        {
                            ++votes[palette];
                            if (++found == 9)
                                break;
                        }
                    }
                    catch (const std::exception &)
                    {
                    }
                }
                if (votes.empty())
                    continue;
                return std::max_element(votes.begin(), votes.end(), [](const auto &a, const auto &b) { return a.second < b.second; })->first;
            }
        }
        return std::nullopt;
    }

    // The unpacked texture of a slot; empty on failure
    static auto Unpack(Reader &art, const Slot &slot) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> packed(slot.Packed - 4);
        art.Seek(static_cast<int64_t>(slot.Offset) + 12);
        art.ReadBuffer(packed.data(), packed.size());
        std::vector<uint8_t> out;
        if (!DclExplode(packed.data(), packed.size(), slot.Unpacked, out) || out.size() != slot.Unpacked)
            out.clear();
        return out;
    }

    std::vector<Slot> _slots;
    std::vector<bool> _rawKinds;
    std::optional<Requiem::Palette> _palette;
    bool _paletteSearched = false;
};
} // namespace

void RegisterRequiemArt()
{
    Register<RequiemArtArchive>("*.ART;*.DAT", "Requiem: Avenging Angel textures (*.ART;*.DAT)");
}
} // namespace Dud
