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
// The vertices of the limbs of a skeletal model follow one another, each limb in its own space; the pose comes
// from an animation (loader and sampler at 0x41E870 / 0x41E3F0). Animation file ("Models/*.ani", or a record of
// AnimFile.bin: the same file): a 0x34 byte header (only the size at 0xC and the frame rate at 0x14 are used), then
// the body, read as 32 bit words from its start:
//   per limb, a 4 bit field of a flags word (a new word is read when the rest of the current one is zero; the
//   low nibble is the first limb): bit 1 rotation, bit 0 rotation animated, bit 3 position, bit 2 position
//   animated. A limb has, in this order:
//   rotation, constant: 4 floats (w, x, y, z). Animated: u32 count, count + 1 key times (frames, the last one is
//     the end), then count keys of 16 floats = 4 rows (cubic, quadratic, linear, constant terms) of (w, x, y, z)
//   position, constant: 3 floats. Animated: count, times, count keys of 12 floats = 4 rows of (x, y, z)
//   The value at frame t (clamped to the first and last time) is ((a3 t + a2) t + a1) t + a0 with t the frame
//   number itself, using the key of the interval that holds it. A limb without rotation (position) has the
//   identity (zero).
//   The count of limbs of a file is whatever makes the limbs end exactly at the end of the body (true for all
//   the 89 .ani files and 499 AnimFile records of the game, with a single count each).
// The skeleton is stored in depth first order by the first u32 of each limb record of the model: bit 0 = the limb
// has children, bits 1 - 15 = number of children (when bit 0 is set) or number of saved rotations to drop (leaf),
// bits 16 - 31 = distance in limbs to the next sibling. The world rotation of a limb is parent * own (quaternions,
// the origin of a child is the parent's origin + parent rotation * the child's position); the rotation and origin
// of the parent are saved on stacks when it has several children and restored at each leaf.
// Which animation poses a model: an animation file named like the model (assault.ani), else the first animation
// of the model in Models3.txt ("M id texture file.mdl", then "A n file.ani"; the record in AnimFile.bin is
// AnimFile.tab[id * 40 + n]); the animation must fit the limbs exactly. A few models have none: they borrow the
// animation of a model of Models3.txt with the same skeleton (same u32 codes), else they are left unposed (every
// limb at the origin). The pose is the first frame of the animation (a walk, for most).
//
// The model is shown as Wavefront OBJ + MTL (the MTL names the texture file; the size of the texture is not in
// the model: texture coordinates are divided by the smallest power of two, 32 or more, that holds them) in the
// pose of the first frame of its first animation; a skeletal model that has animations is also given as a glTF 2.0
// file (one joint per limb, rigid skinning, every animation sampled at 25 frames per second).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <optional>

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
    std::vector<uint32_t> SectionCodes; // skeletal models: first u32 of each limb record (the skeleton)
    std::string PoseSource;             // what posed the model (empty: not posed)
    int ArtSlot = -1;                   // texture slot of Models3.txt
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
        m.SectionCodes.push_back(r.U32());
        r.Skip(0x1C);
        const uint32_t count = r.U32();
        r.Skip(SectionBytes - 0x24);
        m.LimbVertices.push_back(count);
        total += count;
    }
    if (total != vertexCount)
        return false;
    return ReadVertices(r, vertexCount, m) && ReadBody(r, faceCount, flags, (flags & 1) != 0, m);
}

// ---- pose ----

struct Quat
{
    double W = 1, X = 0, Y = 0, Z = 0;
};

using Vec = std::array<double, 3>;

struct LimbPose
{
    Quat Rotation;
    Vec Position{0, 0, 0};
};

auto Multiply(const Quat &p, const Quat &q) -> Quat
{
    return {q.W * p.W - q.X * p.X - q.Y * p.Y - q.Z * p.Z, q.X * p.W + q.W * p.X + q.Z * p.Y - q.Y * p.Z,
            q.Y * p.W + q.W * p.Y + q.X * p.Z - q.Z * p.X, q.Z * p.W + q.Y * p.X + q.W * p.Z - q.X * p.Y};
}

// Rotation matrix of a quaternion (normalized here), rows
using Matrix = std::array<std::array<double, 3>, 3>;

auto ToMatrix(const Quat &q) -> Matrix
{
    const double n = q.W * q.W + q.X * q.X + q.Y * q.Y + q.Z * q.Z;
    const double s = n > 0 ? 2.0 / n : 0.0;
    return {{{1 - s * (q.Y * q.Y + q.Z * q.Z), s * (q.X * q.Y - q.Z * q.W), s * (q.X * q.Z + q.Y * q.W)},
             {s * (q.X * q.Y + q.Z * q.W), 1 - s * (q.X * q.X + q.Z * q.Z), s * (q.Y * q.Z - q.X * q.W)},
             {s * (q.X * q.Z - q.Y * q.W), s * (q.Y * q.Z + q.X * q.W), 1 - s * (q.X * q.X + q.Y * q.Y)}}};
}

auto Rotate(const Matrix &m, const Vec &v) -> Vec
{
    return {m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2], m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
            m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2]};
}

// The words of an animation body; reads past the end fail instead of reading
class Words
{
  public:
    explicit Words(const std::vector<uint8_t> &bytes, size_t start) : _bytes(bytes), _pos(start)
    {
    }
    auto Left() const -> size_t
    {
        return (_bytes.size() - _pos) / 4;
    }
    auto Done() const -> bool
    {
        return _pos == _bytes.size() || _pos + 4 > _bytes.size();
    }
    auto Bad() const -> bool
    {
        return _bad;
    }
    auto U32() -> uint32_t
    {
        if (_pos + 4 > _bytes.size())
        {
            _bad = true;
            return 0;
        }
        uint32_t v;
        std::memcpy(&v, &_bytes[_pos], 4);
        _pos += 4;
        return v;
    }
    auto F32() -> double
    {
        const uint32_t bits = U32();
        float v;
        std::memcpy(&v, &bits, 4);
        return v;
    }
    // The count and the key times of an animated value; false if they do not fit. Keys follow.
    auto Spline(uint32_t keyWords, uint32_t &count, std::vector<double> &times, size_t &keys) -> bool
    {
        count = U32();
        if (_bad || count == 0 || count > 100000 || static_cast<uint64_t>(count) * (keyWords + 1) + 1 > Left())
        {
            _bad = true;
            return false;
        }
        times.resize(count + 1);
        for (double &t : times)
            t = F32();
        keys = _pos;
        _pos += static_cast<size_t>(count) * keyWords * 4;
        return true;
    }
    auto Key(size_t keys, size_t index, uint32_t keyWords, uint32_t word) const -> double
    {
        float v;
        std::memcpy(&v, &_bytes[keys + (static_cast<size_t>(index) * keyWords + word) * 4], 4);
        return v;
    }

  private:
    const std::vector<uint8_t> &_bytes;
    size_t _pos;
    bool _bad = false;
};

// The pose of every limb at frame t of an animation; nullopt unless the limbs use the body exactly. end, if given,
// receives the last key time of the animation
auto SamplePose(const std::vector<uint8_t> &file, size_t limbs, double t, double *end = nullptr)
    -> std::optional<std::vector<LimbPose>>
{
    constexpr size_t HeaderBytes = 0x34;
    if (file.size() < HeaderBytes + 4 || limbs == 0 || limbs > 1000)
        return std::nullopt;
    Words w(file, HeaderBytes);
    std::vector<LimbPose> pose(limbs);
    uint32_t flags = 0;
    std::vector<double> times;
    for (size_t limb = 0; limb < limbs; ++limb)
    {
        if (flags == 0)
            flags = w.U32();
        if (flags & 2)
        {
            if (!(flags & 1))
            {
                pose[limb].Rotation = {w.F32(), w.F32(), w.F32(), w.F32()};
            }
            else
            {
                uint32_t count;
                size_t keys;
                if (!w.Spline(16, count, times, keys))
                    return std::nullopt;
                if (end)
                    *end = std::max(*end, times.back());
                const double tt = std::clamp(t, times.front(), times.back());
                uint32_t k = 0;
                while (k + 1 < count && times[k + 1] < tt)
                    ++k;
                double v[4];
                for (uint32_t j = 0; j < 4; ++j)
                    v[j] = ((tt * w.Key(keys, k, 16, j) + w.Key(keys, k, 16, 4 + j)) * tt + w.Key(keys, k, 16, 8 + j)) * tt +
                           w.Key(keys, k, 16, 12 + j);
                pose[limb].Rotation = {v[0], v[1], v[2], v[3]};
            }
        }
        if (flags & 8)
        {
            if (!(flags & 4))
            {
                pose[limb].Position = {w.F32(), w.F32(), w.F32()};
            }
            else
            {
                uint32_t count;
                size_t keys;
                if (!w.Spline(12, count, times, keys))
                    return std::nullopt;
                if (end)
                    *end = std::max(*end, times.back());
                const double tt = std::clamp(t, times.front(), times.back());
                uint32_t k = 0;
                while (k + 1 < count && times[k + 1] < tt)
                    ++k;
                for (uint32_t j = 0; j < 3; ++j)
                    pose[limb].Position[j] = ((tt * w.Key(keys, k, 12, j) + w.Key(keys, k, 12, 3 + j)) * tt +
                                              w.Key(keys, k, 12, 6 + j)) * tt + w.Key(keys, k, 12, 9 + j);
            }
        }
        flags >>= 4;
    }
    if (w.Bad() || !w.Done())
        return std::nullopt;
    return pose;
}

struct LimbMatrix
{
    Matrix Rotation;
    Vec Origin;
};

// World rotation and origin of every limb (the depth first walk of 0x41E3F0)
auto BuildSkeleton(const std::vector<uint32_t> &codes, const std::vector<LimbPose> &pose) -> std::vector<LimbMatrix>
{
    const size_t n = codes.size();
    std::vector<LimbMatrix> result(n);
    Quat parent;
    Vec origin = pose[0].Position;
    std::vector<Quat> quats;
    std::vector<Vec> origins;
    for (size_t i = 0; i < n; ++i)
    {
        const Quat world = Multiply(parent, pose[i].Rotation);
        const Matrix matrix = ToMatrix(world);
        result[i] = {matrix, origin};
        const uint32_t code = codes[i];
        if (code & 1)
        {
            const uint32_t children = (code >> 1) & 0x7FFF;
            if (children > 1)
            {
                quats.push_back(world);
                // the origins of the other children, the second one on top
                std::vector<size_t> others;
                size_t at = i + 1;
                for (uint32_t c = 1; c < children && at < n; ++c)
                {
                    const uint32_t skip = codes[at] >> 16;
                    if (skip == 0 || at + skip >= n)
                        break;
                    at += skip;
                    others.push_back(at);
                }
                for (auto it = others.rbegin(); it != others.rend(); ++it)
                {
                    const Vec r = Rotate(matrix, pose[*it].Position);
                    origins.push_back({origin[0] + r[0], origin[1] + r[1], origin[2] + r[2]});
                }
            }
            if (i + 1 < n)
            {
                const Vec r = Rotate(matrix, pose[i + 1].Position);
                origin = {origin[0] + r[0], origin[1] + r[1], origin[2] + r[2]};
            }
            parent = world;
        }
        else
        {
            const size_t drop = std::min<size_t>((code >> 1) & 0x7FFF, quats.size());
            quats.resize(quats.size() - drop);
            parent = quats.empty() ? Quat() : quats.back();
            if (!origins.empty())
            {
                origin = origins.back();
                origins.pop_back();
            }
        }
    }
    return result;
}

auto ReadFileBytes(const std::string &path) -> std::vector<uint8_t>
{
    try
    {
        Reader r(path);
        std::vector<uint8_t> bytes(static_cast<size_t>(r.Size()));
        if (!bytes.empty())
            r.ReadBuffer(bytes.data(), bytes.size());
        return bytes;
    }
    catch (const std::exception &)
    {
        return {};
    }
}

// A file of a folder by its name, whatever the case
auto FindInFolder(const std::string &folder, const std::string &name) -> std::string
{
    namespace fs = std::filesystem;
    std::error_code error;
    const std::string wanted = ToLower(name);
    for (const auto &item : fs::directory_iterator(fs::path(std::u8string(folder.begin(), folder.end())), error))
    {
        const std::string file = item.path().filename().string();
        if (ToLower(file) == wanted)
            return item.path().string();
    }
    return {};
}

struct ModelEntry
{
    int Id = 0;
    int Slot = -1;
    std::string File;
    std::vector<std::pair<int, std::string>> Animations;
};

// Models3.txt: "M id texture-slot file.mdl" then "A index file.ani" lines
auto ReadModels3(const std::string &folder) -> std::vector<ModelEntry>
{
    std::vector<ModelEntry> entries;
    const std::string path = FindInFolder(folder, "models3.txt");
    if (path.empty())
        return entries;
    const std::vector<uint8_t> bytes = ReadFileBytes(path);
    const std::string text(bytes.begin(), bytes.end());
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        char kind = 0;
        int a = 0;
        int b = 0;
        char file[256] = {};
        if (line.empty() || line[0] == '#')
            continue;
        if (std::sscanf(line.c_str(), "%c %d %d %255s", &kind, &a, &b, file) == 4 && (kind == 'M' || kind == 'm'))
        {
            ModelEntry e;
            e.Id = a;
            e.Slot = b;
            e.File = file;
            entries.push_back(std::move(e));
        }
        else if (std::sscanf(line.c_str(), "%c %d %255s", &kind, &a, file) == 3 && (kind == 'A' || kind == 'a') &&
                 !entries.empty())
            entries.back().Animations.emplace_back(a, file);
    }
    return entries;
}

// The animation files of AnimFile.bin
class AnimPack
{
  public:
    explicit AnimPack(const std::string &folder)
    {
        const std::string ary = FindInFolder(folder, "animfile.ary");
        const std::string bin = FindInFolder(folder, "animfile.bin");
        const std::string tab = FindInFolder(folder, "animfile.tab");
        if (ary.empty() || bin.empty() || tab.empty())
            return;
        const std::vector<uint8_t> index = ReadFileBytes(ary);
        _bin = ReadFileBytes(bin);
        const std::vector<uint8_t> table = ReadFileBytes(tab);
        if (index.size() < 4 || table.size() < 2)
            return;
        uint32_t count;
        std::memcpy(&count, index.data(), 4);
        if (index.size() != 4 + static_cast<size_t>(count) * 16)
            return;
        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t offset;
            uint32_t size;
            std::memcpy(&offset, &index[4 + i * 16], 4);
            std::memcpy(&size, &index[4 + i * 16 + 8], 4);
            if (static_cast<uint64_t>(offset) + size > _bin.size())
                return;
            _records.emplace_back(offset, size);
        }
        for (size_t i = 0; i + 2 <= table.size(); i += 2)
            _table.push_back(static_cast<uint16_t>(table[i] | table[i + 1] << 8));
    }
    // The animation of an animation index of a model id; empty if there is none
    auto Find(int model, int animation) const -> std::vector<uint8_t>
    {
        const size_t slot = static_cast<size_t>(model) * 40 + static_cast<size_t>(animation);
        if (model < 0 || animation < 0 || animation >= 40 || slot >= _table.size() || _table[slot] >= _records.size())
            return {};
        const auto [offset, size] = _records[_table[slot]];
        return std::vector<uint8_t>(_bin.begin() + offset, _bin.begin() + offset + size);
    }

  private:
    std::vector<uint8_t> _bin;
    std::vector<std::pair<uint32_t, uint32_t>> _records;
    std::vector<uint16_t> _table;
};

// Animations of a model of Models3.txt in the order to try: standing ones first
auto OrderedAnimations(const ModelEntry &e) -> std::vector<std::pair<int, std::string>>
{
    std::vector<std::pair<int, std::string>> list = e.Animations;
    std::stable_partition(list.begin(), list.end(), [](const auto &a) {
        const std::string n = ToLower(a.second);
        return n.find("look") != std::string::npos || n.find("stand") != std::string::npos ||
               n.find("idle") != std::string::npos;
    });
    return list;
}

struct Animation
{
    std::string Name;
    std::vector<uint8_t> Bytes;
};

// The animations that fit the model, the one that poses it first (the rules are in the header comment)
auto CollectAnimations(const Model &m, const std::string &path, int &artSlot) -> std::vector<Animation>
{
    std::vector<Animation> found;
    if (m.SectionCodes.empty())
        return found;
    namespace fs = std::filesystem;
    const std::string folder = fs::path(std::u8string(path.begin(), path.end())).parent_path().string();
    const std::string file = FileNameOf(path);
    const std::string base = ToLower(ChangeFileExt(file, ""));
    const size_t limbs = m.SectionCodes.size();
    const auto add = [&](std::vector<uint8_t> bytes, const std::string &name) {
        if (bytes.empty() || !SamplePose(bytes, limbs, 0.0))
            return;
        for (const Animation &a : found)
            if (a.Name == name)
                return;
        found.push_back({name, std::move(bytes)});
    };

    // 1. an animation file named like the model
    const std::string same = FindInFolder(folder, base + ".ani");
    if (!same.empty())
        add(ReadFileBytes(same), base);

    const std::vector<ModelEntry> entries = ReadModels3(folder);
    const ModelEntry *own = nullptr;
    for (const ModelEntry &e : entries)
        if (ToLower(e.File) == ToLower(file))
            own = &e;
    if (own)
        artSlot = own->Slot;
    std::optional<AnimPack> pack;
    const auto packOf = [&]() -> AnimPack & {
        if (!pack)
            pack.emplace(folder);
        return *pack;
    };
    const auto addEntry = [&](const ModelEntry &e, const std::string &suffix) {
        for (const auto &[index, name] : OrderedAnimations(e))
        {
            const std::string label = ChangeFileExt(name, "") + suffix;
            add(packOf().Find(e.Id, index), label);
            const std::string loose = FindInFolder(folder, name);
            if (!loose.empty())
                add(ReadFileBytes(loose), label);
        }
    };

    // 2. the animations of the model in Models3.txt
    if (own)
        addEntry(*own, "");

    // 3. animation files whose name starts with the one of the model (HandGunG.ani for HandGun.mdl)
    {
        std::error_code error;
        std::vector<std::string> names;
        for (const auto &item : fs::directory_iterator(fs::path(std::u8string(folder.begin(), folder.end())), error))
        {
            const std::string name = item.path().filename().string();
            const std::string low = ToLower(name);
            if (ExtensionOf(low) == ".ani" && low.rfind(base, 0) == 0)
                names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        for (const std::string &name : names)
            add(ReadFileBytes(folder + "/" + name), ChangeFileExt(name, ""));
    }

    // 4. a model with the same skeleton
    if (found.empty())
        for (const ModelEntry &e : entries)
        {
            if (ToLower(e.File) == ToLower(file))
                continue;
            const std::string other = FindInFolder(folder, e.File);
            if (other.empty())
                continue;
            Model o;
            try
            {
                Reader r(other);
                if (!ParseSkeletal(r, o) || o.SectionCodes != m.SectionCodes)
                    continue;
            }
            catch (const std::exception &)
            {
                continue;
            }
            addEntry(e, " (" + ChangeFileExt(e.File, "") + ")");
            if (!found.empty())
                break;
        }
    return found;
}

// Poses the model with the first frame of an animation that fits it; the vertices become world positions
void PoseModel(Model &m, const Animation &animation)
{
    const size_t limbs = m.SectionCodes.size();
    const auto pose = SamplePose(animation.Bytes, limbs, 0.0);
    if (!pose)
        return;
    const std::vector<LimbMatrix> skeleton = BuildSkeleton(m.SectionCodes, *pose);
    size_t first = 0;
    std::vector<size_t> limbOf(m.Vertices.size());
    for (size_t limb = 0; limb < limbs; ++limb)
        for (uint32_t k = 0; k < m.LimbVertices[limb]; ++k, ++first)
            limbOf[first] = limb;
    for (size_t i = 0; i < m.Vertices.size(); ++i)
    {
        const LimbMatrix &lm = skeleton[limbOf[i]];
        const Vec v = Rotate(lm.Rotation, {m.Vertices[i].X, m.Vertices[i].Y, m.Vertices[i].Z});
        m.Vertices[i] = {static_cast<float>(v[0] + lm.Origin[0]), static_cast<float>(v[1] + lm.Origin[1]),
                         static_cast<float>(v[2] + lm.Origin[2])};
    }
    for (size_t i = 0; i < m.Normals.size(); ++i)
    {
        const size_t vertex = m.FaceNormals ? m.Faces[i].Index[0] : i;
        const Vec n = Rotate(skeleton[limbOf[vertex]].Rotation, {m.Normals[i].X, m.Normals[i].Y, m.Normals[i].Z});
        m.Normals[i] = {static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])};
    }
    m.PoseSource = animation.Name;
}

// ---- glTF ----

constexpr double FramesPerSecond = 25.0;

// The parent of every limb (-1 for the root), from the skeleton codes; empty if a limb has none
auto ParentsOf(const std::vector<uint32_t> &codes) -> std::vector<int>
{
    const size_t n = codes.size();
    std::vector<int> parent(n, -2);
    parent[0] = -1;
    for (size_t i = 0; i < n; ++i)
    {
        if (!(codes[i] & 1))
            continue;
        const uint32_t children = (codes[i] >> 1) & 0x7FFF;
        size_t at = i + 1;
        for (uint32_t c = 0; c < children && at < n; ++c)
        {
            parent[at] = static_cast<int>(i);
            const uint32_t skip = codes[at] >> 16;
            if (c + 1 < children)
            {
                if (skip == 0 || at + skip >= n)
                    break;
                at += skip;
            }
        }
    }
    if (std::find(parent.begin(), parent.end(), -2) != parent.end())
        parent.clear();
    return parent;
}

auto Base64(const std::vector<uint8_t> &data) -> std::string
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3)
    {
        const uint32_t v = data[i] << 16 | (i + 1 < data.size() ? data[i + 1] << 8 : 0) | (i + 2 < data.size() ? data[i + 2] : 0);
        out += table[v >> 18 & 63];
        out += table[v >> 12 & 63];
        out += i + 1 < data.size() ? table[v >> 6 & 63] : '=';
        out += i + 2 < data.size() ? table[v & 63] : '=';
    }
    return out;
}

class Gltf
{
  public:
    // A float or index array in the binary buffer; returns the accessor number
    auto Accessor(const void *data, size_t bytes, int component, size_t count, const char *type, const double *min = nullptr,
                  const double *max = nullptr, size_t width = 0, int target = 0) -> int
    {
        while (_buffer.size() % 4 != 0)
            _buffer.push_back(0);
        const size_t offset = _buffer.size();
        _buffer.insert(_buffer.end(), static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + bytes);
        std::string view = "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(bytes);
        if (target)
            view += ",\"target\":" + std::to_string(target);
        _views.push_back(view + "}");
        std::string acc = "{\"bufferView\":" + std::to_string(_views.size() - 1) + ",\"componentType\":" + std::to_string(component) +
                          ",\"count\":" + std::to_string(count) + ",\"type\":\"" + type + "\"";
        if (min && max)
        {
            acc += ",\"min\":[";
            for (size_t i = 0; i < width; ++i)
                acc += (i ? "," : "") + Num(min[i]);
            acc += "],\"max\":[";
            for (size_t i = 0; i < width; ++i)
                acc += (i ? "," : "") + Num(max[i]);
            acc += "]";
        }
        _accessors.push_back(acc + "}");
        return static_cast<int>(_accessors.size() - 1);
    }

    static auto Num(double v) -> std::string
    {
        char b[40];
        std::snprintf(b, sizeof b, "%.9g", v);
        return b;
    }

    std::vector<uint8_t> _buffer;
    std::vector<std::string> _views;
    std::vector<std::string> _accessors;
};

// An animated glTF of a skeletal model: one joint per limb (rigid skinning: the vertices stay in the space of
// their limb, the joint nodes carry the pose), the first fitting animation as the pose of the nodes and all
// the animations sampled at every frame (25 per second). Empty if the skeleton cannot be built.
auto MakeGltf(const Model &m, const std::vector<Animation> &animations, const std::string &base) -> std::string
{
    const size_t limbs = m.SectionCodes.size();
    const std::vector<int> parent = ParentsOf(m.SectionCodes);
    if (parent.empty() || animations.empty() || limbs > 255 || m.Vertices.empty())
        return {};
    std::vector<size_t> limbOf(m.Vertices.size());
    {
        size_t at = 0;
        for (size_t limb = 0; limb < limbs; ++limb)
            for (uint32_t k = 0; k < m.LimbVertices[limb]; ++k)
                limbOf[at++] = limb;
    }

    // one vertex per face corner (the texture coordinates belong to the corner), faces as triangle fans
    std::vector<float> positions;
    std::vector<float> uvs;
    std::vector<float> normals;
    std::vector<uint8_t> joints;
    std::vector<uint32_t> indices;
    double size = 32;
    for (const Face &f : m.Faces)
        for (const auto &uv : f.Uv)
            while (size < std::fabs(uv.first / 65536.0) || size < std::fabs(uv.second / 65536.0))
                size *= 2;
    for (size_t i = 0; i < m.Faces.size(); ++i)
    {
        const Face &f = m.Faces[i];
        const auto first = static_cast<uint32_t>(positions.size() / 3);
        for (size_t k = 0; k < f.Index.size(); ++k)
        {
            const Vec3 &v = m.Vertices[f.Index[k]];
            positions.insert(positions.end(), {v.X, v.Y, v.Z});
            uvs.push_back(static_cast<float>(f.Uv[k].first / 65536.0 / size));
            uvs.push_back(static_cast<float>(f.Uv[k].second / 65536.0 / size));
            if (!m.Normals.empty())
            {
                const Vec3 &n = m.Normals[m.FaceNormals ? i : f.Index[k]];
                normals.insert(normals.end(), {n.X, n.Y, n.Z});
            }
            joints.insert(joints.end(), {static_cast<uint8_t>(limbOf[f.Index[k]]), 0, 0, 0});
        }
        for (size_t k = 2; k < f.Index.size(); ++k)
            indices.insert(indices.end(), {first, first + static_cast<uint32_t>(k - 1), first + static_cast<uint32_t>(k)});
    }
    std::vector<float> weights(positions.size() / 3 * 4, 0.0f);
    for (size_t i = 0; i < weights.size(); i += 4)
        weights[i] = 1.0f;

    Gltf g;
    double lo[3] = {1e30, 1e30, 1e30};
    double hi[3] = {-1e30, -1e30, -1e30};
    for (size_t i = 0; i < positions.size(); ++i)
    {
        lo[i % 3] = std::min<double>(lo[i % 3], positions[i]);
        hi[i % 3] = std::max<double>(hi[i % 3], positions[i]);
    }
    const int accPosition = g.Accessor(positions.data(), positions.size() * 4, 5126, positions.size() / 3, "VEC3", lo, hi, 3, 34962);
    const int accUv = g.Accessor(uvs.data(), uvs.size() * 4, 5126, uvs.size() / 2, "VEC2", nullptr, nullptr, 0, 34962);
    int accNormal = -1;
    if (!normals.empty())
        accNormal = g.Accessor(normals.data(), normals.size() * 4, 5126, normals.size() / 3, "VEC3", nullptr, nullptr, 0, 34962);
    const int accJoints = g.Accessor(joints.data(), joints.size(), 5121, joints.size() / 4, "VEC4", nullptr, nullptr, 0, 34962);
    const int accWeights = g.Accessor(weights.data(), weights.size() * 4, 5126, weights.size() / 4, "VEC4", nullptr, nullptr, 0, 34962);
    const int accIndices = g.Accessor(indices.data(), indices.size() * 4, 5125, indices.size(), "SCALAR", nullptr, nullptr, 0, 34963);

    // the animations
    std::string animationsJson;
    std::vector<LimbPose> rest;
    for (size_t a = 0; a < animations.size(); ++a)
    {
        // frames: from the first to the last key time of the animation
        std::vector<std::vector<LimbPose>> frames;
        double start = 0;
        double end = 0;
        {
            const auto p0 = SamplePose(animations[a].Bytes, limbs, 0.0, &end);
            if (!p0)
                continue;
            if (a == 0)
                rest = *p0;
            const int count = static_cast<int>(std::min(std::max(std::floor(end - start), 0.0), 3000.0)) + 1;
            for (int f = 0; f < count; ++f)
                frames.push_back(*SamplePose(animations[a].Bytes, limbs, start + f));
        }
        std::vector<float> times;
        for (size_t f = 0; f < frames.size(); ++f)
            times.push_back(static_cast<float>(f / FramesPerSecond));
        const float zero = 0.0f;
        const double tmin = 0;
        const double tmax = times.back();
        const double zmax = 0;
        const int accTimes = g.Accessor(times.data(), times.size() * 4, 5126, times.size(), "SCALAR", &tmin, &tmax, 1);
        const int accZero = g.Accessor(&zero, 4, 5126, 1, "SCALAR", &tmin, &zmax, 1);

        std::string samplers;
        std::string channels;
        int sampler = 0;
        const auto emit = [&](const std::vector<float> &values, size_t width, bool rotation, size_t limb) {
            bool constant = true;
            for (size_t f = 1; f < frames.size() && constant; ++f)
                for (size_t c = 0; c < width; ++c)
                    if (values[f * width + c] != values[c])
                        constant = false;
            const int out = g.Accessor(values.data(), (constant ? width : values.size()) * 4, 5126, constant ? 1 : frames.size(),
                                       width == 4 ? "VEC4" : "VEC3");
            samplers += std::string(sampler ? "," : "") + "{\"input\":" + std::to_string(constant ? accZero : accTimes) +
                        ",\"output\":" + std::to_string(out) + ",\"interpolation\":\"LINEAR\"}";
            channels += std::string(sampler ? "," : "") + "{\"sampler\":" + std::to_string(sampler) + ",\"target\":{\"node\":" +
                        std::to_string(limb) + ",\"path\":\"" + (rotation ? "rotation" : "translation") + "\"}}";
            ++sampler;
        };
        for (size_t limb = 0; limb < limbs; ++limb)
        {
            std::vector<float> rot;
            std::vector<float> pos;
            Quat previous{0, 0, 0, 0};
            for (const auto &frame : frames)
            {
                Quat q = frame[limb].Rotation;
                const double n = std::sqrt(q.W * q.W + q.X * q.X + q.Y * q.Y + q.Z * q.Z);
                if (n > 0)
                    q = {q.W / n, q.X / n, q.Y / n, q.Z / n};
                if (q.W * previous.W + q.X * previous.X + q.Y * previous.Y + q.Z * previous.Z < 0)
                    q = {-q.W, -q.X, -q.Y, -q.Z};
                previous = q;
                rot.insert(rot.end(), {static_cast<float>(q.X), static_cast<float>(q.Y), static_cast<float>(q.Z), static_cast<float>(q.W)});
                pos.insert(pos.end(), {static_cast<float>(frame[limb].Position[0]), static_cast<float>(frame[limb].Position[1]),
                                       static_cast<float>(frame[limb].Position[2])});
            }
            emit(pos, 3, false, limb);
            emit(rot, 4, true, limb);
        }
        std::string name = animations[a].Name;
        std::replace(name.begin(), name.end(), '"', '\'');
        animationsJson += std::string(animationsJson.empty() ? "" : ",") + "{\"name\":\"" + name + "\",\"samplers\":[" + samplers +
                          "],\"channels\":[" + channels + "]}";
    }
    if (rest.empty())
        return {};

    // nodes: the limbs, then the skinned mesh
    std::string nodes;
    std::vector<std::vector<size_t>> children(limbs);
    for (size_t i = 1; i < limbs; ++i)
        children[static_cast<size_t>(parent[i])].push_back(i);
    for (size_t i = 0; i < limbs; ++i)
    {
        Quat q = rest[i].Rotation;
        const double n = std::sqrt(q.W * q.W + q.X * q.X + q.Y * q.Y + q.Z * q.Z);
        if (n > 0)
            q = {q.W / n, q.X / n, q.Y / n, q.Z / n};
        nodes += std::string(i ? "," : "") + "{\"name\":\"limb_" + std::to_string(i) + "\",\"translation\":[" + Gltf::Num(rest[i].Position[0]) +
                 "," + Gltf::Num(rest[i].Position[1]) + "," + Gltf::Num(rest[i].Position[2]) + "],\"rotation\":[" + Gltf::Num(q.X) + "," +
                 Gltf::Num(q.Y) + "," + Gltf::Num(q.Z) + "," + Gltf::Num(q.W) + "]";
        if (!children[i].empty())
        {
            nodes += ",\"children\":[";
            for (size_t c = 0; c < children[i].size(); ++c)
                nodes += (c ? "," : "") + std::to_string(children[i][c]);
            nodes += "]";
        }
        nodes += "}";
    }
    nodes += ",{\"name\":\"" + base + "\",\"mesh\":0,\"skin\":0}";
    std::string jointList;
    for (size_t i = 0; i < limbs; ++i)
        jointList += (i ? "," : "") + std::to_string(i);

    std::string attributes = "\"POSITION\":" + std::to_string(accPosition) + ",\"TEXCOORD_0\":" + std::to_string(accUv) +
                             ",\"JOINTS_0\":" + std::to_string(accJoints) + ",\"WEIGHTS_0\":" + std::to_string(accWeights);
    if (accNormal >= 0)
        attributes += ",\"NORMAL\":" + std::to_string(accNormal);
    std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Dragon UnPACKer (Requiem: Avenging Angel model)\"},\"scene\":0,"
                       "\"scenes\":[{\"nodes\":[0," + std::to_string(limbs) + "]}],\"nodes\":[" + nodes + "],"
                       "\"skins\":[{\"joints\":[" + jointList + "],\"skeleton\":0}],"
                       "\"meshes\":[{\"name\":\"" + base + "\",\"primitives\":[{\"attributes\":{" + attributes + "},\"indices\":" +
                       std::to_string(accIndices) + ",\"material\":0}]}],"
                       "\"materials\":[{\"name\":\"texture\",\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":1}"
                       ",\"extras\":{\"artSlot\":" + std::to_string(m.ArtSlot) + ",\"label\":\"" + m.Texture + "\"}}],"
                       "\"animations\":[" + animationsJson + "],\"accessors\":[";
    for (size_t i = 0; i < g._accessors.size(); ++i)
        json += (i ? "," : "") + g._accessors[i];
    json += "],\"bufferViews\":[";
    for (size_t i = 0; i < g._views.size(); ++i)
        json += (i ? "," : "") + g._views[i];
    json += "],\"buffers\":[{\"byteLength\":" + std::to_string(g._buffer.size()) + ",\"uri\":\"data:application/octet-stream;base64," +
            Base64(g._buffer) + "\"}]}";
    return json;
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
    if (!m.PoseSource.empty())
        out += "# posed with the first frame of " + m.PoseSource + "\n";
    else if (!m.LimbVertices.empty())
        out += "# no animation fits this model: every limb is at its own origin\n";
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
        std::string gltf;
        if (!isStatic)
        {
            const std::vector<Animation> animations = CollectAnimations(m, _stream->Path(), m.ArtSlot);
            if (!animations.empty())
            {
                gltf = MakeGltf(m, animations, base);
                PoseModel(m, animations[0]);
            }
        }
        const std::string obj = MakeObj(m, base);
        std::string mtl = "newmtl texture\nKd 1 1 1\n";
        if (m.ArtSlot >= 0)
        {
            // the game loads the picture from this slot of the art archive (the D3D set has the colors)
            char slot[64];
            std::snprintf(slot, sizeof slot, "tex_%04d.tga", m.ArtSlot);
            mtl += "# texture: slot " + std::to_string(m.ArtSlot) + " of the art archive (" + m.Texture + " is only a label)\n";
            mtl += std::string("map_Kd ") + slot + "\n";
        }
        else if (!m.Texture.empty())
            mtl += "map_Kd " + m.Texture + "\n";
        _files.emplace_back(obj.begin(), obj.end());
        _files.emplace_back(mtl.begin(), mtl.end());
        Add(base + ".obj", 0, static_cast<int64_t>(_files[0].size()));
        Add(base + ".mtl", 0, static_cast<int64_t>(_files[1].size()));
        if (!gltf.empty())
        {
            _files.emplace_back(gltf.begin(), gltf.end());
            Add(base + ".gltf", 0, static_cast<int64_t>(_files[2].size()));
        }
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
