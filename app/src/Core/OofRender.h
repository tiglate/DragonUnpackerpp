// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/OofPose.h"
#include "Core/OutrageModel.h"
#include "Core/TgaDecoder.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Dup::Core
{
/// The image of a model texture, found by its name (the TXTR entry, as written): nullptr when there is none.
using OofTextureResolver = std::function<std::shared_ptr<const DecodedImage>(const std::string &)>;

struct OofMaterial
{
    std::string Name;
    std::shared_ptr<const DecodedImage> Image; // nullptr: not found (drawn with a diagnostic checkerboard)
    bool Translucent{false};                   // the image has partial alpha: drawn with blending, after the opaque surfaces
};

/// Everything the viewer needs of a model, built once: the model, its materials, the properties of the subobjects.
struct OofScene
{
    std::shared_ptr<const OofModel> Model;
    std::vector<OofMaterial> Materials; // one per TXTR entry
    std::vector<OofProperties> Properties;
    std::vector<bool> Polygons;         // the subobjects drawn as ordinary polygons
    std::array<double, 3> Center{0, 0, 0}; // of the unanimated pose, in the right-handed coordinates of the view
    double Radius{1};
    bool Animated{false};
    std::size_t ResolvedTextures{0};
    std::size_t MissingTextures{0};
    std::size_t Triangles{0};
};

[[nodiscard]] auto MakeOofScene(std::shared_ptr<const OofModel> model, const OofTextureResolver &resolver) -> std::shared_ptr<const OofScene>;

struct OofCamera
{
    double Yaw{0.6};
    double Pitch{0.4};
    double Zoom{1.0};
    bool CullBackFaces{true}; // the game's one-sided test, with the stored face normals
    bool Wireframe{false};
};

/// Draws the model in a pose with an orthographic camera (the same scale on every axis) into an RGBA picture: surfaces with a
/// depth buffer, then translucent ones back to front, then billboards ($facing) and glows ($glow, $thruster; a generated
/// gradient stands in for the game's effect image). The left-handed file coordinates are mirrored to right-handed ones for the view.
[[nodiscard]] auto RenderOof(const OofScene &scene, const OofPose &pose, const OofCamera &camera, std::uint32_t width, std::uint32_t height,
                             std::array<std::uint8_t, 3> background) -> DecodedImage;
} // namespace Dup::Core
