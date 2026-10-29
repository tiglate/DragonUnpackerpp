// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace Dup::Core
{
/// The geometry of a Wavefront OBJ file: positions and triangles (polygons are split into fans). Normals, texture coordinates
/// and materials are ignored: this is for the 3D preview.
struct Mesh
{
    std::vector<float> Positions;         // x, y, z for each vertex
    std::vector<std::uint32_t> Triangles; // three vertex numbers for each triangle
    float Min[3]{0, 0, 0};
    float Max[3]{0, 0, 0};
};

/// nullopt when the text has no vertex or no face, or more than maxTriangles triangles.
[[nodiscard]] auto ParseObj(std::string_view text, std::size_t maxTriangles = 400000) -> std::optional<Mesh>;
} // namespace Dup::Core
