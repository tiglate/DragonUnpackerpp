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
    bool Wireframe{false};
};

/// Draws the rig in a pose with an orthographic camera (the same scale on every axis, +Y up) into an RGBA picture: triangles
/// with a depth buffer, lit from the camera on both sides, textured with the picture when there is one (texture coordinates
/// times scene.UvSize are texels, repeating), plain gray otherwise.
[[nodiscard]] auto RenderRig(const RigScene &scene, const std::vector<RigJointPose> &pose, const RigCamera &camera, std::uint32_t width,
                             std::uint32_t height, std::array<std::uint8_t, 3> background, const DecodedImage *texture) -> DecodedImage;
} // namespace Dup::Core
