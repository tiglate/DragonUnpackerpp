// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/RigModel.h"

#include "Core/Json.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace Dup::Core
{
namespace
{
constexpr std::size_t MAX_BUFFER_BYTES = 256U * 1024 * 1024;
constexpr std::size_t MAX_JOINTS = 255;

auto DecodeBase64(std::string_view text) -> std::optional<std::vector<std::uint8_t>>
{
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 4 * 3);
    std::uint32_t bits = 0;
    int count = 0;
    for (const char c : text)
    {
        int v;
        if (c >= 'A' && c <= 'Z')
        {
            v = c - 'A';
        }
        else if (c >= 'a' && c <= 'z')
        {
            v = c - 'a' + 26;
        }
        else if (c >= '0' && c <= '9')
        {
            v = c - '0' + 52;
        }
        else if (c == '+' || c == '-')
        {
            v = 62;
        }
        else if (c == '/' || c == '_')
        {
            v = 63;
        }
        else if (c == '=' || c == '\n' || c == '\r' || c == ' ')
        {
            continue;
        }
        else
        {
            return std::nullopt;
        }
        bits = (bits << 6) | static_cast<std::uint32_t>(v);
        if (++count == 4)
        {
            out.push_back(static_cast<std::uint8_t>(bits >> 16));
            out.push_back(static_cast<std::uint8_t>(bits >> 8));
            out.push_back(static_cast<std::uint8_t>(bits));
            bits = 0;
            count = 0;
            if (out.size() > MAX_BUFFER_BYTES)
            {
                return std::nullopt;
            }
        }
    }
    if (count == 2)
    {
        out.push_back(static_cast<std::uint8_t>(bits >> 4));
    }
    else if (count == 3)
    {
        out.push_back(static_cast<std::uint8_t>(bits >> 10));
        out.push_back(static_cast<std::uint8_t>(bits >> 2));
    }
    else if (count == 1)
    {
        return std::nullopt;
    }
    return out;
}

struct AccessorData
{
    std::vector<double> Values;
    std::size_t Count{0};
    std::size_t Width{0};
};

class Document
{
  public:
    Document(const JsonValue &root, std::vector<std::uint8_t> buffer) : _root(root), _buffer(std::move(buffer))
    {
    }

    auto Read(int index) const -> std::optional<AccessorData>
    {
        const auto *accessors = _root.Find("accessors");
        const auto *views = _root.Find("bufferViews");
        if (accessors == nullptr || views == nullptr || index < 0 || static_cast<std::size_t>(index) >= accessors->Items.size())
        {
            return std::nullopt;
        }
        const auto &accessor = accessors->Items[static_cast<std::size_t>(index)];
        const auto *view = accessor.Find("bufferView");
        const auto *component = accessor.Find("componentType");
        const auto *count = accessor.Find("count");
        const auto *type = accessor.Find("type");
        if (view == nullptr || component == nullptr || count == nullptr || type == nullptr || !view->IsNumber() || view->Number < 0 ||
            static_cast<std::size_t>(view->Number) >= views->Items.size())
        {
            return std::nullopt;
        }
        static const std::map<std::string, std::size_t> widths{{"SCALAR", 1}, {"VEC2", 2}, {"VEC3", 3}, {"VEC4", 4}};
        const auto width = widths.find(type->TextOr({}));
        std::size_t size = 0;
        switch (static_cast<int>(component->NumberOr(0)))
        {
        case 5126: case 5125: size = 4; break;
        case 5123: size = 2; break;
        case 5121: size = 1; break;
        default: return std::nullopt;
        }
        if (width == widths.end() || count->Number < 0 || count->Number > 50'000'000)
        {
            return std::nullopt;
        }
        const auto &bufferView = views->Items[static_cast<std::size_t>(view->Number)];
        const auto offset = static_cast<std::size_t>(bufferView.Find("byteOffset") ? bufferView.Find("byteOffset")->NumberOr(0) : 0) +
                            static_cast<std::size_t>(accessor.Find("byteOffset") ? accessor.Find("byteOffset")->NumberOr(0) : 0);
        const auto stride = bufferView.Find("byteStride") ? static_cast<std::size_t>(bufferView.Find("byteStride")->NumberOr(0)) : 0;
        const std::size_t element = width->second * size;
        const std::size_t step = stride != 0 ? stride : element;
        AccessorData data;
        data.Count = static_cast<std::size_t>(count->Number);
        data.Width = width->second;
        if (data.Count != 0 && offset + (data.Count - 1) * step + element > _buffer.size())
        {
            return std::nullopt;
        }
        data.Values.reserve(data.Count * data.Width);
        for (std::size_t i = 0; i < data.Count; ++i)
        {
            for (std::size_t c = 0; c < data.Width; ++c)
            {
                const std::uint8_t *p = &_buffer[offset + i * step + c * size];
                switch (static_cast<int>(component->Number))
                {
                case 5126: {
                    float f;
                    std::memcpy(&f, p, 4);
                    data.Values.push_back(f);
                    break;
                }
                case 5125: {
                    std::uint32_t u;
                    std::memcpy(&u, p, 4);
                    data.Values.push_back(u);
                    break;
                }
                case 5123: data.Values.push_back(static_cast<double>(p[0] | p[1] << 8)); break;
                default: data.Values.push_back(p[0]); break;
                }
            }
        }
        return data;
    }

  private:
    const JsonValue &_root;
    std::vector<std::uint8_t> _buffer;
};

auto Normalized(std::array<float, 4> q) -> std::array<float, 4>
{
    const double n = std::sqrt(static_cast<double>(q[0]) * q[0] + static_cast<double>(q[1]) * q[1] + static_cast<double>(q[2]) * q[2] + static_cast<double>(q[3]) * q[3]);
    if (n < 1e-12)
    {
        return {0, 0, 0, 1};
    }
    return {static_cast<float>(q[0] / n), static_cast<float>(q[1] / n), static_cast<float>(q[2] / n), static_cast<float>(q[3] / n)};
}

/// The rotation of a quaternion (x y z w, normalized here) as a row major matrix.
auto ToMatrix(const std::array<double, 4> &q) -> std::array<double, 9>
{
    const double n = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    const double s = n > 0 ? 2.0 / n : 0.0;
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    return {1 - s * (y * y + z * z), s * (x * y - z * w), s * (x * z + y * w),
            s * (x * y + z * w), 1 - s * (x * x + z * z), s * (y * z - x * w),
            s * (x * z - y * w), s * (y * z + x * w), 1 - s * (x * x + y * y)};
}

auto Multiply(const std::array<double, 9> &a, const std::array<double, 9> &b) -> std::array<double, 9>
{
    std::array<double, 9> r{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            r[static_cast<std::size_t>(i * 3 + j)] = a[static_cast<std::size_t>(i * 3)] * b[static_cast<std::size_t>(j)] + a[static_cast<std::size_t>(i * 3 + 1)] * b[static_cast<std::size_t>(3 + j)] +
                                                     a[static_cast<std::size_t>(i * 3 + 2)] * b[static_cast<std::size_t>(6 + j)];
        }
    }
    return r;
}

auto Rotate(const std::array<double, 9> &m, const std::array<double, 3> &v) -> std::array<double, 3>
{
    return {m[0] * v[0] + m[1] * v[1] + m[2] * v[2], m[3] * v[0] + m[4] * v[1] + m[5] * v[2], m[6] * v[0] + m[7] * v[1] + m[8] * v[2]};
}

/// The segment of a track at a time: the keys on both sides and the fraction between them.
struct Segment
{
    std::size_t A{0};
    std::size_t B{0};
    double F{0};
};

auto Locate(const RigTrack &track, double t) -> Segment
{
    const auto &times = track.Times;
    if (times.size() < 2 || t <= times.front())
    {
        return {0, 0, 0};
    }
    if (t >= times.back())
    {
        return {times.size() - 1, times.size() - 1, 0};
    }
    const auto upper = std::upper_bound(times.begin(), times.end(), static_cast<float>(t));
    const auto b = static_cast<std::size_t>(upper - times.begin());
    const std::size_t a = b - 1;
    const double span = static_cast<double>(times[b]) - times[a];
    return {a, b, span > 0 ? (t - times[a]) / span : 0.0};
}
} // namespace

auto ParseRigGltf(std::string_view text) -> std::optional<RigScene>
{
    const auto root = ParseJson(text);
    if (!root || !root->IsObject())
    {
        return std::nullopt;
    }
    const auto *buffers = root->Find("buffers");
    const auto *nodes = root->Find("nodes");
    const auto *skins = root->Find("skins");
    const auto *meshes = root->Find("meshes");
    if (buffers == nullptr || nodes == nullptr || skins == nullptr || meshes == nullptr || buffers->Items.empty() || skins->Items.empty() || meshes->Items.empty())
    {
        return std::nullopt;
    }
    const auto uri = buffers->Items[0].Find("uri") ? buffers->Items[0].Find("uri")->TextOr({}) : std::string();
    constexpr std::string_view PREFIX = "base64,";
    const auto comma = uri.find(PREFIX);
    if (uri.rfind("data:", 0) != 0 || comma == std::string::npos)
    {
        return std::nullopt; // external or binary buffers are not read
    }
    auto buffer = DecodeBase64(std::string_view(uri).substr(comma + PREFIX.size()));
    if (!buffer)
    {
        return std::nullopt;
    }
    const Document document(*root, std::move(*buffer));

    // the joints
    const auto *jointList = skins->Items[0].Find("joints");
    if (jointList == nullptr || jointList->Items.empty() || jointList->Items.size() > MAX_JOINTS)
    {
        return std::nullopt;
    }
    RigScene scene;
    const std::size_t jointCount = jointList->Items.size();
    std::map<int, int> jointOfNode;
    for (std::size_t j = 0; j < jointCount; ++j)
    {
        const int node = static_cast<int>(jointList->Items[j].NumberOr(-1));
        if (node < 0 || static_cast<std::size_t>(node) >= nodes->Items.size() || jointOfNode.contains(node))
        {
            return std::nullopt;
        }
        jointOfNode[node] = static_cast<int>(j);
    }
    scene.Parent.assign(jointCount, -1);
    scene.RestTranslation.assign(jointCount, {0, 0, 0});
    scene.RestRotation.assign(jointCount, {0, 0, 0, 1});
    for (std::size_t j = 0; j < jointCount; ++j)
    {
        const auto &node = nodes->Items[static_cast<std::size_t>(jointList->Items[j].Number)];
        if (const auto *t = node.Find("translation"); t != nullptr && t->Items.size() == 3)
        {
            scene.RestTranslation[j] = {static_cast<float>(t->Items[0].NumberOr(0)), static_cast<float>(t->Items[1].NumberOr(0)), static_cast<float>(t->Items[2].NumberOr(0))};
        }
        if (const auto *r = node.Find("rotation"); r != nullptr && r->Items.size() == 4)
        {
            scene.RestRotation[j] = Normalized({static_cast<float>(r->Items[0].NumberOr(0)), static_cast<float>(r->Items[1].NumberOr(0)),
                                                static_cast<float>(r->Items[2].NumberOr(0)), static_cast<float>(r->Items[3].NumberOr(1))});
        }
        if (const auto *children = node.Find("children"))
        {
            for (const auto &child : children->Items)
            {
                if (const auto found = jointOfNode.find(static_cast<int>(child.NumberOr(-1))); found != jointOfNode.end())
                {
                    scene.Parent[static_cast<std::size_t>(found->second)] = static_cast<int>(j);
                }
            }
        }
    }
    for (std::size_t j = 0; j < jointCount; ++j)
    {
        if (scene.Parent[j] >= static_cast<int>(j))
        {
            return std::nullopt; // parents must come first (the pose is built in one pass)
        }
    }

    // the mesh
    const auto *primitives = meshes->Items[0].Find("primitives");
    if (primitives == nullptr || primitives->Items.empty())
    {
        return std::nullopt;
    }
    const auto &primitive = primitives->Items[0];
    const auto *attributes = primitive.Find("attributes");
    if (attributes == nullptr || attributes->Find("POSITION") == nullptr || attributes->Find("JOINTS_0") == nullptr || primitive.Find("indices") == nullptr)
    {
        return std::nullopt;
    }
    const auto positions = document.Read(static_cast<int>(attributes->Find("POSITION")->NumberOr(-1)));
    const auto joints = document.Read(static_cast<int>(attributes->Find("JOINTS_0")->NumberOr(-1)));
    const auto indices = document.Read(static_cast<int>(primitive.Find("indices")->NumberOr(-1)));
    if (!positions || !joints || !indices || positions->Width != 3 || joints->Width != 4 || indices->Width != 1 || positions->Count == 0 ||
        joints->Count != positions->Count || indices->Count == 0 || indices->Count % 3 != 0)
    {
        return std::nullopt;
    }
    scene.Positions.assign(positions->Values.begin(), positions->Values.end());
    for (std::size_t v = 0; v < positions->Count; ++v)
    {
        const double joint = joints->Values[v * 4];
        if (joint < 0 || joint >= static_cast<double>(jointCount))
        {
            return std::nullopt;
        }
        scene.JointOf.push_back(static_cast<std::uint8_t>(joint));
    }
    scene.Uvs.assign(positions->Count * 2, 0.0f);
    if (const auto *uv = attributes->Find("TEXCOORD_0"))
    {
        const auto uvs = document.Read(static_cast<int>(uv->NumberOr(-1)));
        if (uvs && uvs->Width == 2 && uvs->Count == positions->Count)
        {
            scene.Uvs.assign(uvs->Values.begin(), uvs->Values.end());
        }
    }
    for (const double index : indices->Values)
    {
        if (index < 0 || index >= static_cast<double>(positions->Count))
        {
            return std::nullopt;
        }
        scene.Indices.push_back(static_cast<std::uint32_t>(index));
    }
    if (const auto *materials = root->Find("materials"); materials != nullptr && !materials->Items.empty())
    {
        if (const auto *extras = materials->Items[0].Find("extras"))
        {
            scene.ArtSlot = static_cast<int>(extras->Find("artSlot") ? extras->Find("artSlot")->NumberOr(-1) : -1);
            scene.UvSize = extras->Find("uvSize") ? std::max(1.0, extras->Find("uvSize")->NumberOr(1)) : 1.0;
            scene.Label = extras->Find("label") ? extras->Find("label")->TextOr({}) : std::string();
        }
    }

    // the animations
    if (const auto *animations = root->Find("animations"))
    {
        for (const auto &animation : animations->Items)
        {
            RigAnimation out;
            out.Name = animation.Find("name") ? animation.Find("name")->TextOr({}) : std::string();
            out.Translation.resize(jointCount);
            out.Rotation.resize(jointCount);
            const auto *samplers = animation.Find("samplers");
            const auto *channels = animation.Find("channels");
            if (samplers == nullptr || channels == nullptr)
            {
                continue;
            }
            bool valid = true;
            for (const auto &channel : channels->Items)
            {
                const auto *target = channel.Find("target");
                const auto *sampler = channel.Find("sampler");
                if (target == nullptr || sampler == nullptr || target->Find("node") == nullptr || target->Find("path") == nullptr ||
                    sampler->Number < 0 || static_cast<std::size_t>(sampler->Number) >= samplers->Items.size())
                {
                    valid = false;
                    break;
                }
                const auto joint = jointOfNode.find(static_cast<int>(target->Find("node")->NumberOr(-1)));
                if (joint == jointOfNode.end())
                {
                    continue;
                }
                const auto &s = samplers->Items[static_cast<std::size_t>(sampler->Number)];
                const auto input = document.Read(static_cast<int>(s.Find("input") ? s.Find("input")->NumberOr(-1) : -1));
                const auto output = document.Read(static_cast<int>(s.Find("output") ? s.Find("output")->NumberOr(-1) : -1));
                const std::string path = target->Find("path")->TextOr({});
                const std::size_t width = path == "rotation" ? 4 : 3;
                if (!input || !output || input->Width != 1 || output->Width != width || input->Count != output->Count || input->Count == 0 ||
                    (path != "rotation" && path != "translation"))
                {
                    valid = false;
                    break;
                }
                RigTrack track;
                track.Times.assign(input->Values.begin(), input->Values.end());
                track.Values.assign(output->Values.begin(), output->Values.end());
                out.Duration = std::max<double>(out.Duration, track.Times.back());
                (path == "rotation" ? out.Rotation : out.Translation)[static_cast<std::size_t>(joint->second)] = std::move(track);
            }
            if (valid)
            {
                scene.Animations.push_back(std::move(out));
            }
        }
    }

    // size of the model, from its rest pose
    const auto rest = EvaluateRig(scene, -1, 0);
    double lo[3] = {1e30, 1e30, 1e30};
    double hi[3] = {-1e30, -1e30, -1e30};
    for (std::size_t v = 0; v < scene.JointOf.size(); ++v)
    {
        const auto &joint = rest[scene.JointOf[v]];
        const auto p = Rotate(joint.R, {scene.Positions[v * 3], scene.Positions[v * 3 + 1], scene.Positions[v * 3 + 2]});
        for (int c = 0; c < 3; ++c)
        {
            const double w = p[static_cast<std::size_t>(c)] + joint.O[static_cast<std::size_t>(c)];
            lo[c] = std::min(lo[c], w);
            hi[c] = std::max(hi[c], w);
        }
    }
    double radius = 0;
    for (int c = 0; c < 3; ++c)
    {
        scene.Center[static_cast<std::size_t>(c)] = (lo[c] + hi[c]) / 2;
        radius = std::max(radius, (hi[c] - lo[c]) / 2);
    }
    scene.Radius = radius > 0 && std::isfinite(radius) ? radius : 1.0;
    return scene;
}

auto EvaluateRig(const RigScene &scene, int animation, double seconds) -> std::vector<RigJointPose>
{
    const std::size_t count = scene.Parent.size();
    std::vector<RigJointPose> pose(count);
    const RigAnimation *anim = animation >= 0 && static_cast<std::size_t>(animation) < scene.Animations.size() ? &scene.Animations[static_cast<std::size_t>(animation)] : nullptr;
    for (std::size_t j = 0; j < count; ++j)
    {
        std::array<double, 3> t{scene.RestTranslation[j][0], scene.RestTranslation[j][1], scene.RestTranslation[j][2]};
        std::array<double, 4> q{scene.RestRotation[j][0], scene.RestRotation[j][1], scene.RestRotation[j][2], scene.RestRotation[j][3]};
        if (anim != nullptr)
        {
            if (const auto &track = anim->Translation[j]; !track.Times.empty())
            {
                const auto s = Locate(track, seconds);
                for (std::size_t c = 0; c < 3; ++c)
                {
                    t[c] = track.Values[s.A * 3 + c] + (track.Values[s.B * 3 + c] - track.Values[s.A * 3 + c]) * s.F;
                }
            }
            if (const auto &track = anim->Rotation[j]; !track.Times.empty())
            {
                const auto s = Locate(track, seconds);
                std::array<double, 4> a{}, b{};
                for (std::size_t c = 0; c < 4; ++c)
                {
                    a[c] = track.Values[s.A * 4 + c];
                    b[c] = track.Values[s.B * 4 + c];
                }
                double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
                if (dot < 0)
                {
                    for (auto &x : b)
                    {
                        x = -x;
                    }
                    dot = -dot;
                }
                double wa = 1.0 - s.F;
                double wb = s.F;
                if (dot < 0.9995)
                {
                    const double angle = std::acos(std::min(dot, 1.0));
                    wa = std::sin((1.0 - s.F) * angle) / std::sin(angle);
                    wb = std::sin(s.F * angle) / std::sin(angle);
                }
                for (std::size_t c = 0; c < 4; ++c)
                {
                    q[c] = a[c] * wa + b[c] * wb;
                }
            }
        }
        const auto local = ToMatrix(q);
        const int parent = scene.Parent[j];
        if (parent < 0)
        {
            pose[j].R = local;
            pose[j].O = t;
        }
        else
        {
            const auto &p = pose[static_cast<std::size_t>(parent)];
            pose[j].R = Multiply(p.R, local);
            const auto d = Rotate(p.R, t);
            pose[j].O = {p.O[0] + d[0], p.O[1] + d[1], p.O[2] + d[2]};
        }
    }
    return pose;
}
} // namespace Dup::Core
