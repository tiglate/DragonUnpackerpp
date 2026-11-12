// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/TgaDecoder.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Dup::Core
{
/// Keys of one animated value of one joint: times in seconds and 3 (translation) or 4 (rotation, x y z w) floats per key.
struct RigTrack
{
    std::vector<float> Times;
    std::vector<float> Values;
};

struct RigAnimation
{
    std::string Name;
    double Duration{0}; // seconds (the last key time of any track)
    std::vector<RigTrack> Translation; // per joint (an empty track keeps the rest value)
    std::vector<RigTrack> Rotation;
};

/// The triangles of one material (one glTF primitive) and its picture.
struct RigPart
{
    std::size_t FirstIndex{0}; // into RigScene::Indices
    std::size_t IndexCount{0};
    int ArtSlot{-1};  // slot of the art archive of the game (-1: none)
    double UvSize{1}; // texture coordinates times this are texels of the picture
    std::string Label;
};

/// A mesh skinned rigidly to a skeleton of joints, and the animations of the joints: what the Requiem models and levels are
/// converted to (glTF 2.0 with embedded data), and what this viewer reads of such a file. Vertices are in the space of their
/// joint (every vertex belongs to one joint), one vertex per face corner. A model has one part (one picture), a level one
/// per texture.
struct RigScene
{
    std::vector<RigPart> Parts; // cover Indices, in order
    std::vector<float> Positions; // x, y, z per vertex, in the space of the joint
    std::vector<float> Uvs;       // u, v per vertex
    std::vector<float> LightUvs;  // u, v per vertex in Lightmap (0 to 1)
    std::shared_ptr<const DecodedImage> Lightmap; // the lighting of a level (multiplies the textures), nullptr: none
    std::vector<std::uint8_t> JointOf;
    std::vector<std::uint32_t> Indices; // three vertices per triangle
    std::vector<int> Parent;            // per joint, -1 for a root; parents come before their children
    std::vector<std::array<float, 3>> RestTranslation;
    std::vector<std::array<float, 4>> RestRotation; // x y z w
    std::vector<RigAnimation> Animations;
    int ArtSlot{-1};       // picture of the first part (the model): slot of the art archive of the game (-1: none)
    double UvSize{1};      // of the first part
    std::string Label;     // what the file calls the texture of the first part
    std::array<double, 3> Center{0, 0, 0}; // of the rest pose
    double Radius{1};
};

/// The joint transform of a pose: p_world = R p + O (R row major).
struct RigJointPose
{
    std::array<double, 9> R{1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::array<double, 3> O{0, 0, 0};
};

/// Reads the glTF text; nullopt when it is not a skinned mesh of the shape described above (or is inconsistent).
[[nodiscard]] auto ParseRigGltf(std::string_view text) -> std::optional<RigScene>;

/// The pose of every joint: animation -1 is the rest pose of the nodes, otherwise that animation at the time (seconds, clamped
/// to its keys). Translations are interpolated linearly, rotations by spherical interpolation.
[[nodiscard]] auto EvaluateRig(const RigScene &scene, int animation, double seconds) -> std::vector<RigJointPose>;
} // namespace Dup::Core
