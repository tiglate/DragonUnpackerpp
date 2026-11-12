// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/RigModel.h"
#include "Core/TgaDecoder.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Dup::Core
{
struct RigCamera
{
    double Yaw{0.0};
    double Pitch{0.15};
    double Zoom{1.0};
    double PanX{0.0}; // pixels the view is moved right
    double PanY{0.0}; // pixels the view is moved down
    bool Wireframe{false};
    bool Lighting{true}; // the lightmaps of the scene (a level) multiply the textures
    double Brightness{2.0}; // the lightmaps times this (the game's own 1x looks too dark on a screen of today)
};

/// Draws the rig in a pose with an orthographic camera (the same scale on every axis, +Y up) into an RGBA picture: triangles
/// with a depth buffer, lit from the camera on both sides, textured with the picture when there is one (texture coordinates
/// times the UvSize of the part are texels, repeating), plain gray otherwise.
/// textures holds the picture of each part of the scene (nullptr or missing: plain gray).
[[nodiscard]] auto RenderRig(const RigScene &scene, const std::vector<RigJointPose> &pose, const RigCamera &camera, std::uint32_t width,
                             std::uint32_t height, std::array<std::uint8_t, 3> background, const std::vector<const DecodedImage *> &textures) -> DecodedImage;
/// The same with one picture for every part.
[[nodiscard]] auto RenderRig(const RigScene &scene, const std::vector<RigJointPose> &pose, const RigCamera &camera, std::uint32_t width,
                             std::uint32_t height, std::array<std::uint8_t, 3> background, const DecodedImage *texture) -> DecodedImage;
} // namespace Dup::Core
