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
// Lightmaps (read from InitTexCache at 0x4F6160 and MarkPolyForUse): the u32 at 12 of the file header is their
// count; the first packed block (level.bin) holds their samples and the count entries of 12 bytes that follow that
// block in the file (the start of rest.bin) describe them: u8 s0, u8 t0, u8 width, u8 height (at most 16), u32 offset
// of the samples in the unpacked block (width x height u16, RGB555 with the top bit set, rows along t), then 4 bytes
// the game fills. The u16 at +0x18 of a piece is its lightmap. The lighting frame of the face (origin at +0x10, axes
// at +0x1C and +0x28, about one sample per world unit) gives each corner its position s, t in samples; the face is
// lit in tiles of 15 samples (the pieces), samples at whole positions: a piece's lightmap starts at (s0, t0) in its
// tile and covers floor(min) to ceil(max) of the piece (checked on dm1 and newpp2: 98% of the sizes are exactly
// that, the others differ by one sample at the border of a tile, so the tile is taken from s0, not recomputed).
// The game multiplies the texture by the lightmap (full white = 31).

#include "RequiemLevel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <optional>

#include "GltfWriter.h"

#include <zlib.h>

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
    out.Lightmap = withUv ? static_cast<int32_t>(*image.U16(at + 0x18)) : -1;
    out.LightUv.clear();
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

constexpr double LightTile = 15.0; // samples per lighting tile of a face

// The coordinates of the corners of a piece in its lightmap (Polygon::LightUv); false if they cannot be had (the
// polygon is then drawn without lightmap)
auto LightCoordinates(const Image &image, size_t face, const std::vector<float> &vertices, const Lightmap &map, Polygon &polygon) -> bool
{
    float frame[9];
    for (size_t i = 0; i < 9; ++i)
        frame[i] = *image.F32(face + 0x10 + i * 4);
    std::vector<double> s;
    std::vector<double> t;
    for (const uint16_t index : polygon.Index)
    {
        const float *v = &vertices[static_cast<size_t>(index) * 3];
        double ds = 0;
        double dt = 0;
        for (size_t c = 0; c < 3; ++c)
        {
            ds += (static_cast<double>(v[c]) - frame[c]) * frame[3 + c];
            dt += (static_cast<double>(v[c]) - frame[c]) * frame[6 + c];
        }
        if (!std::isfinite(ds) || !std::isfinite(dt) || std::fabs(ds) > 1e7 || std::fabs(dt) > 1e7)
            return false;
        s.push_back(ds);
        t.push_back(dt);
    }
    // the tile of the piece: the one where its smallest coordinate is about s0 (t0)
    const double baseS = LightTile * std::round((*std::min_element(s.begin(), s.end()) - map.S0) / LightTile);
    const double baseT = LightTile * std::round((*std::min_element(t.begin(), t.end()) - map.T0) / LightTile);
    for (size_t k = 0; k < s.size(); ++k)
        polygon.LightUv.insert(polygon.LightUv.end(), {static_cast<float>(s[k] - baseS - map.S0), static_cast<float>(t[k] - baseT - map.T0)});
    return true;
}

// A PNG of 8 bit RGB rows (no filter), the pixels being width * height * 3 bytes
auto EncodePng(const std::vector<uint8_t> &rgb, uint32_t width, uint32_t height) -> std::vector<uint8_t>
{
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (width * 3 + 1));
    for (uint32_t y = 0; y < height; ++y)
    {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + static_cast<ptrdiff_t>(static_cast<size_t>(y) * width * 3),
                   rgb.begin() + static_cast<ptrdiff_t>(static_cast<size_t>(y + 1) * width * 3));
    }
    uLongf packedSize = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> packed(packedSize);
    if (compress2(packed.data(), &packedSize, raw.data(), static_cast<uLong>(raw.size()), Z_BEST_SPEED) != Z_OK)
        return {};
    packed.resize(packedSize);
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    const auto chunk = [&png](const char *type, const std::vector<uint8_t> &data) {
        const auto length = static_cast<uint32_t>(data.size());
        png.insert(png.end(), {static_cast<uint8_t>(length >> 24), static_cast<uint8_t>(length >> 16), static_cast<uint8_t>(length >> 8), static_cast<uint8_t>(length)});
        const size_t start = png.size();
        png.insert(png.end(), type, type + 4);
        png.insert(png.end(), data.begin(), data.end());
        const auto crc = static_cast<uint32_t>(crc32(0, png.data() + start, static_cast<uInt>(png.size() - start)));
        png.insert(png.end(), {static_cast<uint8_t>(crc >> 24), static_cast<uint8_t>(crc >> 16), static_cast<uint8_t>(crc >> 8), static_cast<uint8_t>(crc)});
    };
    chunk("IHDR", {static_cast<uint8_t>(width >> 24), static_cast<uint8_t>(width >> 16), static_cast<uint8_t>(width >> 8), static_cast<uint8_t>(width),
                   static_cast<uint8_t>(height >> 24), static_cast<uint8_t>(height >> 16), static_cast<uint8_t>(height >> 8), static_cast<uint8_t>(height), 8, 2, 0, 0, 0});
    chunk("IDAT", packed);
    chunk("IEND", {});
    return png;
}

// The lightmaps in one picture: cells of 18 x 18 samples (the lightmap and a border repeating its edges, so the
// filtering of a viewer does not take the neighbours), cell 0 white (polygons without lightmap)
constexpr uint32_t Cell = 18;

struct Atlas
{
    uint32_t Columns = 1;
    uint32_t Size = Cell; // pixels, square
    std::vector<uint8_t> Rgb;
};

auto MakeAtlas(const std::vector<Lightmap> &maps) -> Atlas
{
    Atlas atlas;
    const size_t cells = maps.size() + 1;
    atlas.Columns = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(cells))));
    atlas.Size = atlas.Columns * Cell;
    atlas.Rgb.assign(static_cast<size_t>(atlas.Size) * atlas.Size * 3, 0);
    for (uint32_t y = 0; y < Cell; ++y)
        std::fill_n(atlas.Rgb.begin() + static_cast<ptrdiff_t>(static_cast<size_t>(y) * atlas.Size * 3), Cell * 3, uint8_t{255});
    for (size_t m = 0; m < maps.size(); ++m)
    {
        const Lightmap &map = maps[m];
        const uint32_t x0 = static_cast<uint32_t>((m + 1) % atlas.Columns) * Cell;
        const uint32_t y0 = static_cast<uint32_t>((m + 1) / atlas.Columns) * Cell;
        for (uint32_t y = 0; y < Cell; ++y)
        {
            const uint32_t row = std::min<uint32_t>(y == 0 ? 0 : y - 1, map.Height - 1u);
            for (uint32_t x = 0; x < Cell; ++x)
            {
                const uint32_t column = std::min<uint32_t>(x == 0 ? 0 : x - 1, map.Width - 1u);
                const uint16_t v = map.Pixels[static_cast<size_t>(row) * map.Width + column];
                uint8_t *p = &atlas.Rgb[(static_cast<size_t>(y0 + y) * atlas.Size + x0 + x) * 3];
                p[0] = static_cast<uint8_t>(((v >> 10) & 31) * 255 / 31);
                p[1] = static_cast<uint8_t>(((v >> 5) & 31) * 255 / 31);
                p[2] = static_cast<uint8_t>((v & 31) * 255 / 31);
            }
        }
    }
    return atlas;
}
} // namespace

auto ReadLightmaps(const std::vector<uint8_t> &table, const std::vector<uint8_t> &samples, uint32_t count, Level &level) -> bool
{
    level.Lightmaps.clear();
    if (count > table.size() / 12)
        return false;
    std::vector<Lightmap> maps(count);
    for (size_t i = 0; i < count; ++i)
    {
        const uint8_t *e = &table[i * 12];
        Lightmap &map = maps[i];
        map.S0 = e[0];
        map.T0 = e[1];
        map.Width = e[2];
        map.Height = e[3];
        const size_t offset = e[4] | e[5] << 8 | e[6] << 16 | static_cast<size_t>(e[7]) << 24;
        const size_t samplesCount = static_cast<size_t>(map.Width) * map.Height;
        if (map.Width == 0 || map.Height == 0 || map.Width > Cell - 2 || map.Height > Cell - 2 || offset > samples.size() ||
            samplesCount * 2 > samples.size() - offset)
            return false;
        map.Pixels.resize(samplesCount);
        for (size_t k = 0; k < samplesCount; ++k)
            map.Pixels[k] = static_cast<uint16_t>(samples[offset + k * 2] | samples[offset + k * 2 + 1] << 8);
    }
    level.Lightmaps = std::move(maps);
    return true;
}

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
                if (read != Outcome::POLYGON)
                    continue;
                if (polygon.Lightmap >= static_cast<int32_t>(level.Lightmaps.size()) ||
                    !LightCoordinates(image, face, out.Vertices, level.Lightmaps[static_cast<size_t>(polygon.Lightmap)], polygon))
                {
                    polygon.Lightmap = -1;
                    polygon.LightUv.clear();
                }
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
    std::vector<float> lightUvs; // in the atlas of the lightmaps (sample i of a lightmap at the centre of its pixel)
    const bool lit = !level.Lightmaps.empty();
    const Atlas atlas = lit ? MakeAtlas(level.Lightmaps) : Atlas();
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
                if (lit)
                {
                    const size_t cell = polygon.LightUv.empty() ? 0 : static_cast<size_t>(polygon.Lightmap) + 1;
                    const double x = static_cast<double>(cell % atlas.Columns * Cell) + 1.5 + (polygon.LightUv.empty() ? 0.0 : polygon.LightUv[k * 2]);
                    const double y = static_cast<double>(cell / atlas.Columns * Cell) + 1.5 + (polygon.LightUv.empty() ? 0.0 : polygon.LightUv[k * 2 + 1]);
                    lightUvs.insert(lightUvs.end(), {static_cast<float>(x / atlas.Size), static_cast<float>(y / atlas.Size)});
                }
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
    std::string attributes = "{\"POSITION\":" + std::to_string(accPosition) + ",\"TEXCOORD_0\":" + std::to_string(accUv) +
                             ",\"JOINTS_0\":" + std::to_string(accJoints) + ",\"WEIGHTS_0\":" + std::to_string(accWeights);
    std::string images;
    std::string occlusion;
    if (lit)
    {
        const int accLight = g.Accessor(lightUvs.data(), lightUvs.size() * 4, 5126, count, "VEC2", nullptr, nullptr, 0, 34962);
        attributes += ",\"TEXCOORD_1\":" + std::to_string(accLight);
        const std::vector<uint8_t> png = EncodePng(atlas.Rgb, atlas.Size, atlas.Size);
        while (g._buffer.size() % 4 != 0)
            g._buffer.push_back(0);
        const size_t offset = g._buffer.size();
        g._buffer.insert(g._buffer.end(), png.begin(), png.end());
        g._views.push_back("{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(png.size()) + "}");
        images = "\"images\":[{\"name\":\"lightmaps\",\"bufferView\":" + std::to_string(g._views.size() - 1) +
                 ",\"mimeType\":\"image/png\"}],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9729,\"wrapS\":33071,\"wrapT\":33071}],"
                 "\"textures\":[{\"source\":0,\"sampler\":0}],";
        occlusion = ",\"occlusionTexture\":{\"index\":0,\"texCoord\":1}";
    }
    attributes += "}";
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
                     "\",\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":1},\"doubleSided\":true" + occlusion + ",\"extras\":{" + extras + "}}";
        ++material;
    }
    std::string label = name;
    std::replace(label.begin(), label.end(), '"', '\'');
    std::replace(label.begin(), label.end(), '\\', '/');
    std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Dragon UnPACKer (Requiem: Avenging Angel level)\"},\"scene\":0,"
                       "\"scenes\":[{\"nodes\":[0,1]}],\"nodes\":[{\"name\":\"root\"},{\"name\":\"" + label + "\",\"mesh\":0,\"skin\":0}],"
                       "\"skins\":[{\"joints\":[0],\"skeleton\":0}],"
                       "\"meshes\":[{\"name\":\"" + label + "\",\"primitives\":[" + primitives + "]}],\"materials\":[" + materials + "]," + images + "\"accessors\":[";
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
