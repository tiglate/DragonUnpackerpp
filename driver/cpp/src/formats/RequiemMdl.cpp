// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Requiem: Avenging Angel models (Models/*.mdl), found by reading the game's own loaders (3DFX.exe) and checked
// against every file of the game: the layouts below account for each file byte for byte. The file is the dump of
// the in-memory structures (the pointers in it are junk and are not used). Little endian.
//
// Static model (props, items, weapons in the world), loader at 0x441E90:
//   u8 flags; u32 nameLength; name (texture file name, null terminated); u32 vertexCount; u32 faceCount;
//   vertexCount x (float x, y, z); faceCount x face; normals
// Skeletal model (characters, first person weapons), loaders 0x41F5F0 - 0x41F950:
//   u32 sectionCount; u32 pointer; u32 faceCount; u32 pointer; u8 flags at 0x10 (3 bytes padding); u16 vertexCount
//   at 0x14; u16 nameLength at 0x16; 24 more bytes (mostly pointers); the name; sectionCount x 0x38 bytes
//   (limbs: u16 and s16 at 0, u32 vertex count of the limb at 0x20, the rest is pointers and zeros);
//   vertexCount x (float x, y, z); faceCount x face; normals
// face: u32 n; 16 bytes of pointers; n x u16 vertex index; n x (s32 u, s32 v) texture coordinates in 16.16 fixed
//   point texels (0.5 is the center of the first texel)
// normals: flags & 2: none. Static models: one (float x, y, z) per face (flags are 1, 2, 4 or 0x82 in the game's
//   files; the loader at 0x441E90 would read vertex normals for 4, the three files with that value have face
//   normals). Skeletal models: one per face if flags & 1, otherwise one per vertex (flags are 0x10 in the game).
// The vertices of the limbs of a skeletal model follow one another, each limb in its own space: the position of
// the limbs comes from the animation (the .ani files, AnimFile.bin), which is not read here, so these models are
// shown with every limb as a group at its own origin.
//
// The model is shown as Wavefront OBJ + MTL (the MTL names the texture file; the size of the texture is not in
// the model: texture coordinates are divided by the smallest power of two, 32 or more, that holds them).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxCount = 1000000;
constexpr size_t SectionBytes = 0x38;
constexpr size_t SkeletalHeader = 0x2C;

struct Vec3
{
    float X = 0;
    float Y = 0;
    float Z = 0;
};

struct Face
{
    std::vector<uint16_t> Index;
    std::vector<std::pair<int32_t, int32_t>> Uv;
};

struct Model
{
    std::string Texture;
    std::vector<Vec3> Vertices;
    std::vector<Face> Faces;
    std::vector<Vec3> Normals;
    bool FaceNormals = false;
    std::vector<uint32_t> LimbVertices; // skeletal models: vertices of each limb
};

auto ReadFloat(Reader &r) -> float
{
    const uint32_t bits = r.U32();
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// Faces, then the normals; false unless the file ends right there
auto ReadBody(Reader &r, uint32_t faceCount, uint8_t flags, bool faceNormals, Model &m) -> bool
{
    const int64_t size = r.Size();
    m.Faces.reserve(faceCount);
    for (uint32_t i = 0; i < faceCount; ++i)
    {
        if (r.Pos() + 20 > size)
            return false;
        const uint32_t n = r.U32();
        r.Skip(16);
        if (n < 3 || n > 64 || r.Pos() + static_cast<int64_t>(n) * 10 > size)
            return false;
        Face face;
        for (uint32_t k = 0; k < n; ++k)
        {
            const uint16_t index = r.U16();
            if (index >= m.Vertices.size())
                return false;
            face.Index.push_back(index);
        }
        for (uint32_t k = 0; k < n; ++k)
        {
            const int32_t u = r.I32();
            const int32_t v = r.I32();
            face.Uv.emplace_back(u, v);
        }
        m.Faces.push_back(std::move(face));
    }
    if (flags & 2)
        return r.Pos() == size;
    m.FaceNormals = faceNormals;
    const size_t count = m.FaceNormals ? faceCount : m.Vertices.size();
    if (r.Pos() + static_cast<int64_t>(count) * 12 != size)
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        Vec3 n;
        n.X = ReadFloat(r);
        n.Y = ReadFloat(r);
        n.Z = ReadFloat(r);
        m.Normals.push_back(n);
    }
    return true;
}

auto ReadVertices(Reader &r, uint32_t count, Model &m) -> bool
{
    if (r.Pos() + static_cast<int64_t>(count) * 12 > r.Size())
        return false;
    for (uint32_t i = 0; i < count; ++i)
    {
        Vec3 v;
        v.X = ReadFloat(r);
        v.Y = ReadFloat(r);
        v.Z = ReadFloat(r);
        m.Vertices.push_back(v);
    }
    return true;
}

auto ParseStatic(Reader &r, Model &m) -> bool
{
    r.Seek(0);
    if (r.Size() < 14)
        return false;
    const uint8_t flags = r.U8();
    const uint32_t nameLength = r.U32();
    if (nameLength == 0 || nameLength > 128 || r.Pos() + nameLength + 8 > r.Size())
        return false;
    std::string name(nameLength, '\0');
    r.ReadBuffer(name.data(), nameLength);
    if (name.back() != '\0')
        return false;
    m.Texture = name.c_str();
    const uint32_t vertexCount = r.U32();
    const uint32_t faceCount = r.U32();
    if (vertexCount == 0 || vertexCount > 65535 || faceCount == 0 || faceCount > MaxCount)
        return false;
    return ReadVertices(r, vertexCount, m) && ReadBody(r, faceCount, flags, true, m);
}

auto ParseSkeletal(Reader &r, Model &m) -> bool
{
    r.Seek(0);
    if (r.Size() < static_cast<int64_t>(SkeletalHeader))
        return false;
    const uint32_t sectionCount = r.U32();
    r.U32();
    const uint32_t faceCount = r.U32();
    r.U32();
    const uint8_t flags = r.U8();
    r.Skip(3);
    const uint16_t vertexCount = r.U16();
    const uint16_t nameLength = r.U16();
    r.Skip(SkeletalHeader - 0x18);
    if (sectionCount == 0 || sectionCount > 1000 || faceCount == 0 || faceCount > MaxCount || vertexCount == 0 ||
        nameLength == 0 || nameLength > 128 ||
        r.Pos() + nameLength + static_cast<int64_t>(sectionCount) * SectionBytes > r.Size())
        return false;
    std::string name(nameLength, '\0');
    r.ReadBuffer(name.data(), nameLength);
    m.Texture = name.c_str();
    uint32_t total = 0;
    for (uint32_t i = 0; i < sectionCount; ++i)
    {
        r.Skip(0x20);
        const uint32_t count = r.U32();
        r.Skip(SectionBytes - 0x24);
        m.LimbVertices.push_back(count);
        total += count;
    }
    if (total != vertexCount)
        return false;
    return ReadVertices(r, vertexCount, m) && ReadBody(r, faceCount, flags, (flags & 1) != 0, m);
}

void Put(std::string &out, const char *format, double a, double b = 0, double c = 0)
{
    char buffer[96];
    const int n = std::snprintf(buffer, sizeof buffer, format, a, b, c);
    out.append(buffer, static_cast<size_t>(n));
}

// The OBJ text; texture coordinates are scaled to 0..1 by the smallest power of two that holds them
auto MakeObj(const Model &m, const std::string &base) -> std::string
{
    double maxUv = 0;
    for (const Face &f : m.Faces)
        for (const auto &uv : f.Uv)
            maxUv = std::max({maxUv, std::fabs(uv.first / 65536.0), std::fabs(uv.second / 65536.0)});
    double size = 32;
    while (size < maxUv)
        size *= 2;

    std::string out = "# Requiem: Avenging Angel model, converted by Dragon UnPACKer\n";
    out += "mtllib " + base + ".mtl\nusemtl texture\n";
    for (const Vec3 &v : m.Vertices)
        Put(out, "v %.6f %.6f %.6f\n", v.X, v.Y, v.Z);
    for (const Face &f : m.Faces)
        for (const auto &uv : f.Uv)
            Put(out, "vt %.6f %.6f\n", uv.first / 65536.0 / size, 1.0 - uv.second / 65536.0 / size);
    for (const Vec3 &n : m.Normals)
        Put(out, "vn %.6f %.6f %.6f\n", n.X, n.Y, n.Z);

    // limb of each vertex
    std::vector<int> limbOf;
    for (size_t limb = 0; limb < m.LimbVertices.size(); ++limb)
        limbOf.insert(limbOf.end(), m.LimbVertices[limb], static_cast<int>(limb));
    int group = -2;
    size_t uvIndex = 1;
    for (size_t i = 0; i < m.Faces.size(); ++i)
    {
        const Face &f = m.Faces[i];
        const int limb = limbOf.empty() ? -1 : limbOf[f.Index[0]];
        if (limb != group)
        {
            group = limb;
            out += limb < 0 ? "g model\n" : "g limb_" + std::to_string(limb) + "\n";
        }
        out += "f";
        for (size_t k = 0; k < f.Index.size(); ++k)
        {
            out += " " + std::to_string(f.Index[k] + 1) + "/" + std::to_string(uvIndex++);
            if (!m.Normals.empty())
                out += "/" + std::to_string((m.FaceNormals ? i : f.Index[k]) + 1);
        }
        out += "\n";
    }
    return out;
}

class RequiemMdlArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        Model m;
        return ParseStatic(stream, m) || (m = Model(), ParseSkeletal(stream, m));
    }

    void Parse() override
    {
        Model m;
        const bool isStatic = ParseStatic(*_stream, m);
        if (!isStatic)
        {
            m = Model();
            if (!ParseSkeletal(*_stream, m))
                Fail(DUDI7_ERR_NOTFORMAT, "Not a Requiem model");
        }
        _formatId = isStatic ? "Requiem MDL" : "Requiem MDL (skeletal)";
        _dirSeparator = "";
        _extractInternal = true;
        const std::string base = ChangeFileExt(FileNameOf(_stream->Path()), "");
        const std::string obj = MakeObj(m, base);
        std::string mtl = "newmtl texture\nKd 1 1 1\n";
        if (!m.Texture.empty())
            mtl += "map_Kd " + m.Texture + "\n";
        _files.emplace_back(obj.begin(), obj.end());
        _files.emplace_back(mtl.begin(), mtl.end());
        Add(base + ".obj", 0, static_cast<int64_t>(_files[0].size()));
        Add(base + ".mtl", 0, static_cast<int64_t>(_files[1].size()));
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const auto &bytes = _files[static_cast<size_t>(index)];
        return ExtractBytes(bytes.data(), static_cast<int64_t>(bytes.size()), sink);
    }

  private:
    std::vector<std::vector<uint8_t>> _files;
};
} // namespace

void RegisterRequiemMdl()
{
    Register<RequiemMdlArchive>("*.MDL", "Requiem: Avenging Angel models (*.MDL)");
}
} // namespace Dud
