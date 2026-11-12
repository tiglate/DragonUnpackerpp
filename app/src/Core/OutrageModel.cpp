// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Descent 3 .oof models ("PSPO" chunks), written from docs/file-formats-graphics-and-models.md of the Descent 3 repository.

#include "Core/OutrageModel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <stdexcept>

namespace Dup::Core
{
namespace
{
class Failure : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

class Cursor
{
  public:
    explicit Cursor(std::span<const std::byte> data) : _data(data)
    {
    }
    [[nodiscard]] auto Remaining() const -> std::size_t
    {
        return _data.size() - _pos;
    }
    auto U8() -> std::uint8_t
    {
        Need(1);
        return static_cast<std::uint8_t>(_data[_pos++]);
    }
    auto I32() -> std::int32_t
    {
        Need(4);
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
        {
            v |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(_data[_pos + static_cast<std::size_t>(i)])) << (8 * i);
        }
        _pos += 4;
        return static_cast<std::int32_t>(v);
    }
    auto F32() -> float
    {
        const auto bits = static_cast<std::uint32_t>(I32());
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    auto Vec3() -> OofVec3
    {
        OofVec3 v;
        v[0] = F32();
        v[1] = F32();
        v[2] = F32();
        return v;
    }
    /// A position or offset: it must be a number (normals of degenerate faces are NaN in real files, and are not used).
    auto Point() -> OofVec3
    {
        const auto v = Vec3();
        if (!std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2]))
        {
            throw Failure("non-finite coordinate");
        }
        return v;
    }
    /// A count that must be non-negative and whose records (of at least recordBytes) fit in what is left.
    auto Count(std::size_t recordBytes) -> std::size_t
    {
        const std::int32_t n = I32();
        if (n < 0 || (recordBytes != 0 && static_cast<std::size_t>(n) > Remaining() / recordBytes))
        {
            throw Failure("a count does not fit in its chunk");
        }
        return static_cast<std::size_t>(n);
    }
    /// ModelString: a byte count, then the bytes; the text stops at the first zero.
    auto String() -> std::string
    {
        const std::size_t n = Count(1);
        Need(n);
        std::string text;
        for (std::size_t i = 0; i < n && _data[_pos + i] != std::byte{0}; ++i)
        {
            text += static_cast<char>(_data[_pos + i]);
        }
        _pos += n;
        return text;
    }

  private:
    void Need(std::size_t n) const
    {
        if (n > Remaining())
        {
            throw Failure("truncated chunk");
        }
    }
    std::span<const std::byte> _data;
    std::size_t _pos{0};
};

auto Id(std::span<const std::byte> data, std::size_t at) -> std::string
{
    return {reinterpret_cast<const char *>(data.data()) + at, 4};
}

auto ReadPoints(Cursor &in, bool withParent) -> std::vector<OofPoint>
{
    std::vector<OofPoint> points(in.Count(24));
    for (auto &p : points)
    {
        p.Parent = withParent ? in.I32() : 0;
        p.Position = in.Point();
        p.Normal = in.Vec3();
    }
    return points;
}

void ReadSubobject(Cursor &in, OofModel &model, std::vector<bool> &seen)
{
    const std::int32_t index = in.I32();
    if (index < 0 || static_cast<std::size_t>(index) >= model.Subobjects.size())
    {
        throw Failure("subobject index out of range");
    }
    if (seen[static_cast<std::size_t>(index)])
    {
        throw Failure("subobject index used twice");
    }
    seen[static_cast<std::size_t>(index)] = true;
    OofSubobject &so = model.Subobjects[static_cast<std::size_t>(index)];
    const int major = model.Version / 100;
    so.Parent = in.I32();
    if (so.Parent == index)
    {
        so.Parent = -1; // the loader turns a subobject that is its own parent into a root
    }
    so.PlaneNormal = in.Vec3();
    so.PlaneDistance = in.F32();
    so.PlanePoint = in.Vec3();
    so.Offset = in.Point();
    so.Radius = in.F32();
    in.I32(); // tree offset
    in.I32(); // data offset
    so.GeometricCenter = in.Vec3();
    so.Name = in.String();
    so.Properties = in.String();
    so.MovementType = in.I32();
    so.MovementAxis = in.I32();
    const std::size_t freeSpace = in.Count(4);
    for (std::size_t i = 0; i < freeSpace; ++i)
    {
        in.I32();
    }
    const std::size_t vertices = in.Count(24);
    so.Positions.resize(vertices);
    so.Normals.resize(vertices);
    for (auto &p : so.Positions)
    {
        p = in.Point();
    }
    for (auto &n : so.Normals)
    {
        n = in.Vec3();
    }
    if (major >= 23)
    {
        so.Alpha.resize(vertices);
        for (auto &a : so.Alpha)
        {
            a = in.F32();
        }
    }
    so.Faces.resize(in.Count(20));
    for (auto &face : so.Faces)
    {
        face.Normal = in.Vec3();
        const std::size_t corners = in.Count(12);
        if (in.I32() != 0)
        {
            face.Texture = in.I32();
        }
        else
        {
            face.Texture = -1;
            for (auto &c : face.Color)
            {
                c = in.U8();
            }
        }
        face.Corners.resize(corners);
        for (auto &corner : face.Corners)
        {
            corner.Vertex = in.I32();
            corner.U = in.F32();
            corner.V = in.F32();
            if (corner.Vertex < 0 || static_cast<std::size_t>(corner.Vertex) >= vertices)
            {
                throw Failure("face refers to a vertex that does not exist");
            }
        }
        if (major >= 21)
        {
            in.F32();
            in.F32();
        }
    }
}

void ReadRotation(Cursor &in, OofModel &model)
{
    const int major = model.Version / 100;
    const std::size_t subobjects = model.Subobjects.size();
    model.Rotation.assign(subobjects, {});
    if (major <= 21)
    {
        const std::size_t keys = in.Count(0);
        if (subobjects != 0 && keys > in.Remaining() / 16 / subobjects)
        {
            throw Failure("animation does not fit in its chunk");
        }
        for (auto &track : model.Rotation)
        {
            for (std::size_t k = 0; k < keys; ++k)
            {
                OofRotationKey key;
                key.Tick = static_cast<std::int32_t>(k); // the old profile has no times
                key.Axis = in.Vec3();
                key.Angle = in.I32();
                track.Keys.push_back(key);
            }
        }
        return;
    }
    for (auto &track : model.Rotation)
    {
        track.Timed = true;
        const std::size_t keys = in.Count(0);
        track.MinTick = in.I32();
        track.MaxTick = in.I32();
        if (keys > in.Remaining() / 20)
        {
            throw Failure("animation does not fit in its chunk");
        }
        for (std::size_t k = 0; k < keys; ++k)
        {
            OofRotationKey key;
            key.Tick = in.I32();
            key.Axis = in.Vec3();
            key.Angle = in.I32();
            track.Keys.push_back(key);
        }
    }
}

void ReadPositionAnimation(Cursor &in, OofModel &model)
{
    const int major = model.Version / 100;
    const std::size_t subobjects = model.Subobjects.size();
    model.Position.assign(subobjects, {});
    if (major <= 21)
    {
        const std::size_t keys = in.Count(0);
        if (subobjects != 0 && keys > in.Remaining() / 12 / subobjects)
        {
            throw Failure("animation does not fit in its chunk");
        }
        for (auto &track : model.Position)
        {
            for (std::size_t k = 0; k < keys; ++k)
            {
                track.Keys.push_back({static_cast<std::int32_t>(k), in.Vec3()});
            }
        }
        return;
    }
    for (auto &track : model.Position)
    {
        track.Timed = true;
        const std::size_t keys = in.Count(0);
        track.MinTick = in.I32();
        track.MaxTick = in.I32();
        if (keys > in.Remaining() / 16)
        {
            throw Failure("animation does not fit in its chunk");
        }
        for (std::size_t k = 0; k < keys; ++k)
        {
            OofPositionKey key;
            key.Tick = in.I32();
            key.Position = in.Vec3();
            track.Keys.push_back(key);
        }
    }
}

void Validate(OofModel &model, const std::vector<bool> &seen)
{
    const auto count = static_cast<std::int32_t>(model.Subobjects.size());
    for (std::int32_t i = 0; i < count; ++i)
    {
        if (!seen[static_cast<std::size_t>(i)])
        {
            throw Failure("a subobject is missing");
        }
        const auto &so = model.Subobjects[static_cast<std::size_t>(i)];
        if (so.Parent < -1 || so.Parent >= count)
        {
            throw Failure("parent out of range");
        }
        std::int32_t steps = 0;
        for (std::int32_t p = so.Parent; p != -1; p = model.Subobjects[static_cast<std::size_t>(p)].Parent)
        {
            if (++steps > count)
            {
                throw Failure("cycle in the subobject hierarchy");
            }
        }
        for (const auto &face : so.Faces)
        {
            if (face.Texture >= static_cast<std::int32_t>(model.Textures.size()) || face.Texture < -1)
            {
                throw Failure("face refers to a texture that does not exist");
            }
            if (so.Positions.size() >= 2500 || face.Corners.size() >= 100 || model.Textures.size() >= 35)
            {
                model.Notes.push_back("exceeds the engine limits (2500 vertices, 100 corners, 35 textures)");
            }
        }
    }
    for (const auto &list : {&model.Gunpoints, &model.Attachments, &model.GroundContacts})
    {
        for (const auto &p : *list)
        {
            if (p.Parent < 0 || p.Parent >= count)
            {
                throw Failure("a point belongs to a subobject that does not exist");
            }
        }
    }
    for (const auto &battery : model.Batteries)
    {
        for (const auto g : battery.Gunpoints)
        {
            if (g < 0 || static_cast<std::size_t>(g) >= model.Gunpoints.size())
            {
                throw Failure("battery refers to a missing gunpoint");
            }
        }
        for (const auto t : battery.Turrets)
        {
            if (t < 0 || t >= count)
            {
                throw Failure("battery refers to a missing turret");
            }
        }
        if (battery.Gunpoints.size() > 8 || battery.Turrets.size() > 8)
        {
            model.Notes.push_back("a battery has more than 8 gunpoints or turrets: the game keeps the first eight");
        }
    }
    for (const auto &track : model.Rotation)
    {
        for (std::size_t k = 1; k < track.Keys.size(); ++k)
        {
            if (track.Timed && track.Keys[k].Tick <= track.Keys[k - 1].Tick)
            {
                model.Notes.push_back("animation key times are not increasing");
            }
        }
    }
    for (const auto &track : model.Position)
    {
        for (std::size_t k = 1; k < track.Keys.size(); ++k)
        {
            if (track.Timed && track.Keys[k].Tick <= track.Keys[k - 1].Tick)
            {
                model.Notes.push_back("animation key times are not increasing");
            }
        }
    }
    std::sort(model.Notes.begin(), model.Notes.end());
    model.Notes.erase(std::unique(model.Notes.begin(), model.Notes.end()), model.Notes.end());
}

auto Finite(float v) -> double
{
    return std::isfinite(v) ? static_cast<double>(v) : 0.0;
}

/// Offset of a subobject from the model origin: the sum of its own offset and its ancestors'.
auto Placement(const OofModel &model, std::size_t index) -> OofVec3
{
    OofVec3 total{};
    std::size_t steps = 0;
    for (std::int32_t i = static_cast<std::int32_t>(index); i != -1 && steps <= model.Subobjects.size(); ++steps)
    {
        const auto &so = model.Subobjects[static_cast<std::size_t>(i)];
        for (int c = 0; c < 3; ++c)
        {
            total[static_cast<std::size_t>(c)] += so.Offset[static_cast<std::size_t>(c)];
        }
        i = so.Parent;
    }
    return total;
}

auto HasText(const OofSubobject &so, const char *name) -> bool
{
    std::string text = so.Properties;
    for (auto &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text.find(name) != std::string::npos;
}

/// Which subobjects the ordinary polygon renderer draws as authored polygons. Door helpers ($shell, $frontface) and nodes without
/// faces are pruned with their children by the reference renderer; $facing, $glow, $thruster (effects drawn from the origin) and
/// $custom (needs an effect) have no ordinary polygons. The OHDR detail list plays no part: every root is drawn.
auto DrawnSetImpl(const OofModel &model, bool pruneSubtrees) -> std::vector<bool>
{
    std::vector<bool> drawn(model.Subobjects.size(), true);
    std::vector<bool> pruned(model.Subobjects.size(), false);
    for (std::size_t i = 0; i < drawn.size(); ++i)
    {
        const auto &so = model.Subobjects[i];
        const auto properties = ParseOofProperties(so.Properties);
        const bool helper = properties.Shell || properties.FrontFace || so.Faces.empty();
        if (helper)
        {
            drawn[i] = false;
            pruned[i] = pruneSubtrees;
        }
        else if (properties.Facing || properties.Glow || properties.Thruster || properties.Custom || HasText(so, "$glow") || HasText(so, "$thruster"))
        {
            drawn[i] = false;
        }
    }
    for (std::size_t i = 0; i < drawn.size(); ++i)
    {
        for (std::int32_t p = model.Subobjects[i].Parent; p != -1; p = model.Subobjects[static_cast<std::size_t>(p)].Parent)
        {
            if (pruned[static_cast<std::size_t>(p)])
            {
                drawn[i] = false;
            }
        }
    }
    return drawn;
}
} // namespace

auto ParseOofProperties(const std::string &text) -> OofProperties
{
    std::string lower = text;
    for (auto &c : lower)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    OofProperties result;
    auto has = [&](const char *name) { return lower.find(name) != std::string::npos; };
    // the numbers after "name=": as many as the property has, separated by commas
    auto numbers = [&](const char *name, std::size_t count) -> std::optional<std::vector<double>> {
        const auto at = lower.find(name);
        if (at == std::string::npos)
        {
            return std::nullopt;
        }
        const char *p = lower.c_str() + at + std::strlen(name);
        std::vector<double> values;
        while (values.size() < count)
        {
            char *end = nullptr;
            const double v = std::strtod(p, &end);
            if (end == p || !std::isfinite(v))
            {
                return std::nullopt;
            }
            values.push_back(v);
            p = end;
            while (*p == ',' || *p == ' ')
            {
                ++p;
            }
        }
        return values;
    };
    result.Jitter = has("$jitter");
    result.Shell = has("$shell");
    result.Facing = has("$facing");
    result.FrontFace = has("$frontface");
    result.Viewer = has("$viewer");
    result.Layer = has("$layer");
    result.Custom = has("$custom");
    for (int m = 1; m <= 8; ++m)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "$monitor0%d", m);
        if (has(name))
        {
            result.Monitor = m;
        }
    }
    if (const auto v = numbers("$rotate=", 1); v && (*v)[0] > 0 && (*v)[0] <= 20)
    {
        result.RotateSeconds = (*v)[0];
    }
    if (const auto v = numbers("$fov=", 3))
    {
        result.Fov = std::array<double, 3>{(*v)[0], (*v)[1], (*v)[2]};
    }
    if (const auto v = numbers("$glow=", 4))
    {
        result.Glow = std::array<double, 4>{(*v)[0], (*v)[1], (*v)[2], (*v)[3]};
    }
    if (const auto v = numbers("$thruster=", 4))
    {
        result.Thruster = std::array<double, 4>{(*v)[0], (*v)[1], (*v)[2], (*v)[3]};
    }
    return result;
}

auto OofDrawnSubobjects(const OofModel &model, bool pruneSubtrees) -> std::vector<bool>
{
    return DrawnSetImpl(model, pruneSubtrees);
}

auto IsOof(std::span<const std::byte> data) -> bool
{
    return data.size() >= 12 && std::memcmp(data.data(), "PSPO", 4) == 0;
}

auto ParseOof(std::span<const std::byte> data, std::string *error) -> std::optional<OofModel>
{
    try
    {
        if (!IsOof(data))
        {
            throw Failure("not a PSPO model");
        }
        OofModel model;
        Cursor header(data.subspan(4, 4));
        model.RawVersion = header.I32();
        model.Version = model.RawVersion < 18 ? model.RawVersion * 100 : model.RawVersion;
        if (model.Version < 1807 || model.Version > 2300)
        {
            throw Failure("unsupported model version " + std::to_string(model.RawVersion));
        }
        std::vector<bool> seen;
        bool haveHeader = false;
        std::size_t at = 8;
        while (data.size() - at >= 8)
        {
            const auto id = Id(data, at);
            Cursor length(data.subspan(at + 4, 4));
            const std::int32_t size = length.I32();
            at += 8;
            if (size < 0 || static_cast<std::size_t>(size) > data.size() - at)
            {
                throw Failure("a chunk goes past the end of the file");
            }
            Cursor in(data.subspan(at, static_cast<std::size_t>(size)));
            at += static_cast<std::size_t>(size);
            const bool needsHeader = id == "SOBJ" || id == "RANI" || id == "ANIM" || id == "PANI" || id == "GPNT" || id == "ATCH" || id == "GRND" || id == "WBAT";
            if (needsHeader && !haveHeader)
            {
                throw Failure("chunk " + id + " before the object header");
            }
            if (id == "OHDR")
            {
                if (haveHeader)
                {
                    throw Failure("two object headers");
                }
                const std::size_t subobjects = in.Count(0);
                if (subobjects > 100000)
                {
                    throw Failure("too many subobjects");
                }
                model.Radius = in.F32();
                model.BoxMin = in.Vec3();
                model.BoxMax = in.Vec3();
                model.Details.resize(in.Count(4));
                for (auto &d : model.Details)
                {
                    d = in.I32();
                }
                model.Subobjects.resize(subobjects);
                seen.assign(subobjects, false);
                haveHeader = true;
            }
            else if (id == "SOBJ")
            {
                ReadSubobject(in, model, seen);
            }
            else if (id == "TXTR")
            {
                const std::size_t count = in.Count(4);
                for (std::size_t i = 0; i < count; ++i)
                {
                    model.Textures.push_back(in.String());
                }
            }
            else if (id == "GPNT")
            {
                model.Gunpoints = ReadPoints(in, model.Version >= 1908);
            }
            else if (id == "ATCH")
            {
                model.Attachments = ReadPoints(in, true);
            }
            else if (id == "GRND")
            {
                model.GroundContacts = ReadPoints(in, true);
            }
            else if (id == "NATH")
            {
                const std::size_t count = in.Count(28);
                if (count != model.Attachments.size())
                {
                    throw Failure("NATH count differs from ATCH");
                }
                for (std::size_t i = 0; i < count; ++i)
                {
                    in.I32();
                    in.Vec3();
                    model.AttachmentUp.push_back(in.Vec3());
                }
            }
            else if (id == "WBAT")
            {
                model.Batteries.resize(in.Count(8));
                for (auto &battery : model.Batteries)
                {
                    battery.Gunpoints.resize(in.Count(4));
                    for (auto &g : battery.Gunpoints)
                    {
                        g = in.I32();
                    }
                    battery.Turrets.resize(in.Count(4));
                    for (auto &t : battery.Turrets)
                    {
                        t = in.I32();
                    }
                }
            }
            else if (id == "RANI" || id == "ANIM")
            {
                ReadRotation(in, model);
            }
            else if (id == "PANI")
            {
                ReadPositionAnimation(in, model);
            }
            // IDTA, PINF, GRID and unknown chunks: skipped by their length
        }
        if (!haveHeader)
        {
            throw Failure("no object header");
        }
        Validate(model, seen);
        return model;
    }
    catch (const std::exception &failure)
    {
        if (error != nullptr)
        {
            *error = failure.what();
        }
        return std::nullopt;
    }
}

auto ExportOofObj(const OofModel &model) -> std::string
{
    std::string text = "# Descent 3 model, version " + std::to_string(model.RawVersion) + "\n";
    text += "# Z is negated (the game is left-handed with +Z forward), the winding is reversed to match and V is flipped (0 is the bottom)\n";
    for (std::size_t t = 0; t < model.Textures.size(); ++t)
    {
        text += "# texture " + std::to_string(t) + ": " + model.Textures[t] + "\n";
    }
    const std::vector<bool> keep(model.Subobjects.size(), true);
    char line[160];
    std::size_t vBase = 1, vtBase = 1;
    for (std::size_t s = 0; s < model.Subobjects.size(); ++s)
    {
        if (!keep[s])
        {
            continue;
        }
        const auto &so = model.Subobjects[s];
        const auto at = Placement(model, s);
        text += "o " + (so.Name.empty() ? "subobject" + std::to_string(s) : so.Name) + "\n";
        if (!so.Properties.empty())
        {
            text += "# properties: " + so.Properties + "\n";
        }
        for (const auto &p : so.Positions)
        {
            std::snprintf(line, sizeof(line), "v %.6g %.6g %.6g\n", static_cast<double>(p[0] + at[0]), static_cast<double>(p[1] + at[1]), 0.0 - static_cast<double>(p[2] + at[2]));
            text += line;
        }
        for (const auto &n : so.Normals)
        {
            std::snprintf(line, sizeof(line), "vn %.6g %.6g %.6g\n", Finite(n[0]), Finite(n[1]), 0.0 - Finite(n[2]));
            text += line;
        }
        std::size_t uvs = 0;
        for (const auto &face : so.Faces)
        {
            for (const auto &c : face.Corners)
            {
                std::snprintf(line, sizeof(line), "vt %.6g %.6g\n", Finite(c.U), 1.0 - Finite(c.V));
                text += line;
            }
            uvs += face.Corners.size();
        }
        std::size_t uv = vtBase;
        std::int32_t lastTexture = -2;
        std::array<std::uint8_t, 3> lastColor{};
        for (const auto &face : so.Faces)
        {
            if (face.Texture != lastTexture || (face.Texture < 0 && face.Color != lastColor))
            {
                if (face.Texture >= 0)
                {
                    text += "usemtl " + model.Textures[static_cast<std::size_t>(face.Texture)] + "\n";
                }
                else
                {
                    std::snprintf(line, sizeof(line), "usemtl rgb_%02x%02x%02x\n", face.Color[0], face.Color[1], face.Color[2]);
                    text += line;
                }
                lastTexture = face.Texture;
                lastColor = face.Color;
            }
            text += "f";
            for (std::size_t k = face.Corners.size(); k-- > 0;)
            {
                const auto v = vBase + static_cast<std::size_t>(face.Corners[k].Vertex);
                text += " " + std::to_string(v) + "/" + std::to_string(uv + k) + "/" + std::to_string(v);
            }
            uv += face.Corners.size();
            text += "\n";
        }
        vBase += so.Positions.size();
        vtBase += uvs;
    }
    return text;
}

auto MakeOofMesh(const OofModel &model, bool pruneSubtrees, std::size_t maxTriangles) -> std::optional<Mesh>
{
    const auto keep = DrawnSetImpl(model, pruneSubtrees);
    Mesh mesh;
    bool first = true;
    for (std::size_t s = 0; s < model.Subobjects.size(); ++s)
    {
        if (!keep[s])
        {
            continue;
        }
        const auto &so = model.Subobjects[s];
        const auto at = Placement(model, s);
        const auto base = static_cast<std::uint32_t>(mesh.Positions.size() / 3);
        for (const auto &p : so.Positions)
        {
            for (std::size_t c = 0; c < 3; ++c)
            {
                const float value = (p[c] + at[c]) * (c == 2 ? -1.0f : 1.0f); // left-handed to right-handed
                mesh.Positions.push_back(value);
                if (first)
                {
                    mesh.Min[c] = mesh.Max[c] = value;
                }
                mesh.Min[c] = std::min(mesh.Min[c], value);
                mesh.Max[c] = std::max(mesh.Max[c], value);
            }
            first = false;
        }
        for (const auto &face : so.Faces)
        {
            for (std::size_t k = 2; k < face.Corners.size(); ++k)
            {
                mesh.Triangles.push_back(base + static_cast<std::uint32_t>(face.Corners[0].Vertex));
                mesh.Triangles.push_back(base + static_cast<std::uint32_t>(face.Corners[k].Vertex)); // reversed with the Z flip
                mesh.Triangles.push_back(base + static_cast<std::uint32_t>(face.Corners[k - 1].Vertex));
            }
        }
        if (mesh.Triangles.size() / 3 > maxTriangles)
        {
            return std::nullopt;
        }
    }
    if (mesh.Triangles.empty())
    {
        return std::nullopt;
    }
    return mesh;
}

auto DescribeOof(const OofModel &model) -> std::string
{
    std::size_t faces = 0;
    for (const auto &so : model.Subobjects)
    {
        faces += so.Faces.size();
    }
    std::string text = std::to_string(model.Subobjects.size()) + " subobjects, " + std::to_string(faces) + " faces";
    if (!model.Textures.empty())
    {
        text += "; textures:";
        for (const auto &t : model.Textures)
        {
            text += " " + t;
        }
    }
    for (const auto &note : model.Notes)
    {
        text += "; " + note;
    }
    return text;
}
} // namespace Dup::Core
