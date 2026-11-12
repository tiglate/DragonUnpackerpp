// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The geometry of a Requiem: Avenging Angel level, read from the second unpacked block of a .jam file (data.bin),
// and written as glTF. The layout is in RequiemLevel.cpp.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Dud::RequiemLevel
{
struct Polygon
{
    uint32_t Texture = 0;          // slot of the art archive (tex_NNNN)
    std::vector<uint16_t> Index;   // into the vertices of its object
    std::vector<float> Uv;         // two per corner, texels of the texture (empty: none)
    int32_t Lightmap = -1;         // into Level::Lightmaps (-1: none)
    std::vector<float> LightUv;    // two per corner, in samples of the lightmap (0: the first sample)
};

/// A lightmap of the first block of the level (level.bin): Width x Height RGB555 samples, rows along t
struct Lightmap
{
    uint8_t S0 = 0; // the first sample in the lightmap tile of its face (samples at whole positions)
    uint8_t T0 = 0;
    uint8_t Width = 0;
    uint8_t Height = 0;
    std::vector<uint16_t> Pixels;
};

struct Object
{
    std::vector<float> Vertices; // x, y, z
    std::vector<Polygon> Polygons;
};

struct Level
{
    std::vector<Object> Objects;
    std::vector<Lightmap> Lightmaps;
};

// The lightmaps: count entries of 12 bytes (table, the bytes after the first packed block) whose samples are in the
// unpacked first block; false (and none) if an entry does not fit
auto ReadLightmaps(const std::vector<uint8_t> &table, const std::vector<uint8_t> &samples, uint32_t count, Level &level) -> bool;

// The level geometry of a data block; false if the block does not have the layout described in RequiemLevel.cpp. The
// lightmaps are read first (ReadLightmaps): the polygons get their coordinates in them.
auto Read(const std::vector<uint8_t> &data, Level &level) -> bool;

// The level as glTF: one primitive per texture, one joint (the viewer of the program reads skinned meshes); the
// materials carry the art slot in their extras. With lightmaps, they are packed in one picture (PNG in the buffer),
// given to every material as its occlusion texture with the coordinates TEXCOORD_1. Empty if the level has no polygon.
auto MakeGltf(const Level &level, const std::string &name) -> std::string;
} // namespace Dud::RequiemLevel
