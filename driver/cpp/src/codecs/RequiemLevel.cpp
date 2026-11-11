// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The level geometry of Requiem: Avenging Angel (read from the game's level loader, LoadJAM at 0x4606E0 of
// 3DFX.exe, its object pass at 0x462749, and the renderer: 0x447150, 0x447CA0, 0x460650, MarkPolyForUse at
// 0x4F68D0).
//
// The second unpacked block of a .jam file (data.bin, see formats/RequiemJam.cpp) is a memory image of the game's
// objects: the game reads it whole into its own memory, which starts at 0x06000000, so every pointer of the block
// is 0x06000000 + an offset into the block (0 = none). Little endian.
//
// - u32 at 0: the size of the trailer at the end of the block. The trailer lists the objects by class: for each
//   class number from 0 up, a u32 count and that many pointers; it ends exactly at the end of the block. An object
//   of a derived class is listed under each of its classes.
// - Class 3 objects hold geometry: u32 at +0x40 = pointer to the vertices (three floats each, world space, +Y up),
//   u16 at +0x44 = vertex count, u16 at +0x46 = face count, u32 at +0x48 = pointer to the faces (0x68 bytes
//   each). Objects without vertices are the other things of the level (lights, sounds, ...).
// - Class 108 objects are the textures: u32 at 0 = slot of the art archive (tex_NNNN). The art index has 4001
//   slots: the game makes the textures of the slots above 4000 at run time (another path of the loader, 0x41FBD0),
//   so they have no picture (the material says gameTexture in place of artSlot).
// - Face: plane (normal, distance), an origin and two axes (the lighting frame), then at +0x3C its polygon (below,
//   no texture coordinates), at +0x5C a pointer to its pieces and at +0x60 their count (u8). The pieces are the
//   face cut along the borders of the texture repeats: each lies in one repeat and has texture coordinates, in
//   texels of the texture of the software set (0 to its size, kept 0.05 away from the borders; the 3dfx loader
//   doubles them for its 0-256 range). The same sizes hold for the D3D set.
// - Polygon (0x20 bytes): pointer to its u16 vertex indices at +4, pointer to its corner data at +8 (16 bytes per
//   corner: two floats u, v, then two that the game fills), pointer to the texture at +0xC, u16 lightmap number at
//   +0x18, corner count at +0x1A (u8), flags at +0x1B. V counts from the first stored row of the art TGA (its
//   bottom row), as in the models.
// The lightmaps (the first block, level.bin) are not read.

#include "RequiemLevel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <optional>

#include "GltfWriter.h"

namespace Dud::RequiemLevel
{
namespace
{
constexpr uint32_t Base = 0x06000000u;
constexpr size_t GeometryClass = 3;
constexpr size_t TextureClass = 108;
constexpr size_t FaceBytes = 0x68;
constexpr size_t PolygonBytes = 0x20;
constexpr size_t FacePolygon = 0x3C;
constexpr size_t MaxClasses = 4096;
constexpr uint32_t MaxArtSlot = 4000; // the art index has 4001 slots

class Image
{
  public:
    explicit Image(const std::vector<uint8_t> &data) : _data(data)
    {
    }

    auto Fits(size_t offset, size_t bytes) const -> bool
    {
        return offset <= _data.size() && bytes <= _data.size() - offset;
    }

    auto U8(size_t offset) const -> std::optional<uint8_t>
    {
        if (!Fits(offset, 1))
            return std::nullopt;
        return _data[offset];
    }

    auto U16(size_t offset) const -> std::optional<uint16_t>
    {
        if (!Fits(offset, 2))
            return std::nullopt;
        return static_cast<uint16_t>(_data[offset] | _data[offset + 1] << 8);
    }

    auto U32(size_t offset) const -> std::optional<uint32_t>
    {
        if (!Fits(offset, 4))
            return std::nullopt;
        return static_cast<uint32_t>(_data[offset] | _data[offset + 1] << 8 | _data[offset + 2] << 16) |
               static_cast<uint32_t>(_data[offset + 3]) << 24;
    }

    auto F32(size_t offset) const -> std::optional<float>
    {
        const auto bits = U32(offset);
        if (!bits)
            return std::nullopt;
        float f;
        std::memcpy(&f, &*bits, 4);
        return f;
    }

    // The offset of a pointer of the image, if it points to bytes of the image
    auto Offset(uint32_t pointer, size_t bytes) const -> std::optional<size_t>
    {
        if (pointer < Base)
            return std::nullopt;
        const size_t offset = pointer - Base;
        if (!Fits(offset, bytes))
            return std::nullopt;
        return offset;
    }

    auto Pointer(size_t at, size_t bytes) const -> std::optional<size_t>
    {
        const auto p = U32(at);
        if (!p)
            return std::nullopt;
        return Offset(*p, bytes);
    }

  private:
    const std::vector<uint8_t> &_data;
};

// The pointers of each class of the trailer; empty if the trailer does not end exactly at the end of the block
auto ReadClasses(const Image &image, size_t size) -> std::vector<std::vector<uint32_t>>
{
    std::vector<std::vector<uint32_t>> classes;
    const auto trailer = image.U32(0);
    if (!trailer || *trailer < 4 || *trailer > size - 4)
        return {};
    size_t at = size - *trailer;
    while (at < size)
    {
        const auto count = image.U32(at);
        if (!count || classes.size() >= MaxClasses || *count > (size - at - 4) / 4)
            return {};
        at += 4;
        std::vector<uint32_t> pointers(*count);
        for (auto &p : pointers)
        {
            p = *image.U32(at);
            at += 4;
        }
        classes.push_back(std::move(pointers));
    }
    return at == size ? classes : std::vector<std::vector<uint32_t>>();
}

enum class Outcome
{
    POLYGON,
    NONE, // no texture: not drawn (the game has such faces without pieces)
    BAD
};

// One polygon of a face (the face's own polygon or one of its pieces)
auto ReadPolygon(const Image &image, size_t at, size_t vertices, bool withUv, Polygon &out) -> Outcome
{
    const auto count = image.U8(at + 0x1A);
    const auto pointer = image.U32(at + 0xC);
    if (!count || !pointer)
        return Outcome::BAD;
    if (*pointer == 0)
        return Outcome::NONE;
    const auto texture = image.Offset(*pointer, 4);
    if (*count < 3 || !texture)
        return Outcome::BAD;
    const auto index = image.Pointer(at + 4, static_cast<size_t>(*count) * 2);
    if (!index)
        return Outcome::BAD;
    out.Texture = *image.U32(*texture);
    out.Index.resize(*count);
    for (size_t k = 0; k < *count; ++k)
    {
        out.Index[k] = *image.U16(*index + k * 2);
        if (out.Index[k] >= vertices)
            return Outcome::BAD;
    }
    out.Uv.clear();
    if (withUv)
    {
        const auto corners = image.Pointer(at + 8, static_cast<size_t>(*count) * 16);
        if (!corners)
            return Outcome::BAD;
        for (size_t k = 0; k < *count; ++k)
        {
            const float u = *image.F32(*corners + k * 16);
            const float v = *image.F32(*corners + k * 16 + 4);
            if (!std::isfinite(u) || !std::isfinite(v))
                return Outcome::BAD;
            out.Uv.insert(out.Uv.end(), {u, v});
        }
    }
    return Outcome::POLYGON;
}
} // namespace

auto Read(const std::vector<uint8_t> &data, Level &level) -> bool
{
    level.Objects.clear();
    const Image image(data);
    const auto classes = ReadClasses(image, data.size());
    if (classes.size() <= std::max(GeometryClass, TextureClass))
        return false;
    for (const uint32_t pointer : classes[GeometryClass])
    {
        const auto object = image.Offset(pointer, 0x4C);
        if (!object)
            return false;
        const size_t vertexCount = *image.U16(*object + 0x44);
        const size_t faceCount = *image.U16(*object + 0x46);
        if (vertexCount == 0 || faceCount == 0)
            continue;
        const auto vertices = image.Pointer(*object + 0x40, vertexCount * 12);
        const auto faces = image.Pointer(*object + 0x48, faceCount * FaceBytes);
        if (!vertices || !faces)
            return false;
        Object out;
        out.Vertices.resize(vertexCount * 3);
        for (size_t i = 0; i < out.Vertices.size(); ++i)
        {
            out.Vertices[i] = *image.F32(*vertices + i * 4);
            if (!std::isfinite(out.Vertices[i]))
                return false;
        }
        for (size_t f = 0; f < faceCount; ++f)
        {
            const size_t face = *faces + f * FaceBytes;
            const size_t pieces = *image.U8(face + 0x60);
            Polygon polygon;
            if (pieces == 0)
            {
                // a face that was not cut has no texture coordinates: shown with its texture at the origin
                const auto read = ReadPolygon(image, face + FacePolygon, vertexCount, false, polygon);
                if (read == Outcome::BAD)
                    return false;
                if (read == Outcome::POLYGON)
                    out.Polygons.push_back(std::move(polygon));
                continue;
            }
            const auto first = image.Pointer(face + 0x5C, pieces * PolygonBytes);
            if (!first)
                return false;
            for (size_t p = 0; p < pieces; ++p)
            {
                const auto read = ReadPolygon(image, *first + p * PolygonBytes, vertexCount, true, polygon);
                if (read == Outcome::BAD)
                    return false;
                if (read == Outcome::POLYGON)
                    out.Polygons.push_back(polygon);
            }
        }
        level.Objects.push_back(std::move(out));
    }
    return true;
}

auto MakeGltf(const Level &level, const std::string &name) -> std::string
{
    // one vertex per polygon corner (texture coordinates belong to the corner), polygons as triangle fans, grouped
    // by texture; the texture coordinates are texels / 128 with V negated (V counts from the bottom row of the
    // picture: -v repeats into the right row whatever its height), and the material says 128 texels per unit
    std::vector<float> positions;
    std::vector<float> uvs;
    std::map<uint32_t, std::vector<uint32_t>> indices;
    for (const Object &object : level.Objects)
    {
        for (const Polygon &polygon : object.Polygons)
        {
            const auto first = static_cast<uint32_t>(positions.size() / 3);
            for (size_t k = 0; k < polygon.Index.size(); ++k)
            {
                const float *v = &object.Vertices[static_cast<size_t>(polygon.Index[k]) * 3];
                positions.insert(positions.end(), {v[0], v[1], v[2]});
                const float u = polygon.Uv.empty() ? 0.0f : polygon.Uv[k * 2];
                const float t = polygon.Uv.empty() ? 0.0f : polygon.Uv[k * 2 + 1];
                uvs.insert(uvs.end(), {u / 128.0f, -t / 128.0f});
            }
            auto &list = indices[polygon.Texture];
            for (size_t k = 2; k < polygon.Index.size(); ++k)
                list.insert(list.end(), {first, first + static_cast<uint32_t>(k - 1), first + static_cast<uint32_t>(k)});
        }
    }
    if (positions.empty())
        return {};
    const size_t count = positions.size() / 3;
    std::vector<uint8_t> joints(count * 4, 0);
    std::vector<float> weights(count * 4, 0.0f);
    for (size_t i = 0; i < weights.size(); i += 4)
        weights[i] = 1.0f;

    GltfWriter g;
    double lo[3] = {1e30, 1e30, 1e30};
    double hi[3] = {-1e30, -1e30, -1e30};
    for (size_t i = 0; i < positions.size(); ++i)
    {
        lo[i % 3] = std::min<double>(lo[i % 3], positions[i]);
        hi[i % 3] = std::max<double>(hi[i % 3], positions[i]);
    }
    const int accPosition = g.Accessor(positions.data(), positions.size() * 4, 5126, count, "VEC3", lo, hi, 3, 34962);
    const int accUv = g.Accessor(uvs.data(), uvs.size() * 4, 5126, count, "VEC2", nullptr, nullptr, 0, 34962);
    const int accJoints = g.Accessor(joints.data(), joints.size(), 5121, count, "VEC4", nullptr, nullptr, 0, 34962);
    const int accWeights = g.Accessor(weights.data(), weights.size() * 4, 5126, count, "VEC4", nullptr, nullptr, 0, 34962);
    const std::string attributes = "{\"POSITION\":" + std::to_string(accPosition) + ",\"TEXCOORD_0\":" + std::to_string(accUv) +
                                   ",\"JOINTS_0\":" + std::to_string(accJoints) + ",\"WEIGHTS_0\":" + std::to_string(accWeights) + "}";
    std::string primitives;
    std::string materials;
    size_t material = 0;
    for (const auto &[texture, list] : indices)
    {
        const int accIndices = g.Accessor(list.data(), list.size() * 4, 5125, list.size(), "SCALAR", nullptr, nullptr, 0, 34963);
        primitives += std::string(material ? "," : "") + "{\"attributes\":" + attributes + ",\"indices\":" + std::to_string(accIndices) +
                      ",\"material\":" + std::to_string(material) + "}";
        // the art archive has slots up to 4000: the game makes the textures above at run time (sky, effects)
        const std::string extras = texture <= MaxArtSlot ? "\"artSlot\":" + std::to_string(texture) + ",\"uvSize\":128"
                                                         : "\"artSlot\":-1,\"gameTexture\":" + std::to_string(texture);
        materials += std::string(material ? "," : "") + "{\"name\":\"tex_" + std::to_string(texture) +
                     "\",\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":1},\"doubleSided\":true,\"extras\":{" + extras + "}}";
        ++material;
    }
    std::string label = name;
    std::replace(label.begin(), label.end(), '"', '\'');
    std::replace(label.begin(), label.end(), '\\', '/');
    std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Dragon UnPACKer (Requiem: Avenging Angel level)\"},\"scene\":0,"
                       "\"scenes\":[{\"nodes\":[0,1]}],\"nodes\":[{\"name\":\"root\"},{\"name\":\"" + label + "\",\"mesh\":0,\"skin\":0}],"
                       "\"skins\":[{\"joints\":[0],\"skeleton\":0}],"
                       "\"meshes\":[{\"name\":\"" + label + "\",\"primitives\":[" + primitives + "]}],\"materials\":[" + materials + "],\"accessors\":[";
    for (size_t i = 0; i < g._accessors.size(); ++i)
        json += (i ? "," : "") + g._accessors[i];
    json += "],\"bufferViews\":[";
    for (size_t i = 0; i < g._views.size(); ++i)
        json += (i ? "," : "") + g._views[i];
    json += "],\"buffers\":[{\"byteLength\":" + std::to_string(g._buffer.size()) + ",\"uri\":\"data:application/octet-stream;base64," +
            Base64(g._buffer) + "\"}]}";
    return json;
}
} // namespace Dud::RequiemLevel
