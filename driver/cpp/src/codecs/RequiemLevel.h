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
};

struct Object
{
    std::vector<float> Vertices; // x, y, z
    std::vector<Polygon> Polygons;
};

struct Level
{
    std::vector<Object> Objects;
};

// The level geometry of a data block; false if the block does not have the layout described in RequiemLevel.cpp
auto Read(const std::vector<uint8_t> &data, Level &level) -> bool;

// The level as glTF: one primitive per texture, one joint (the viewer of the program reads skinned meshes); the
// materials carry the art slot in their extras. Empty if the level has no polygon.
auto MakeGltf(const Level &level, const std::string &name) -> std::string;
} // namespace Dud::RequiemLevel
