// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/OutrageModel.h"

#include <array>
#include <string>
#include <vector>

namespace Dup::Core
{
/// A rigid transform in the coordinates of the file (+X right, +Y up, +Z forward), column vectors: p' = R p + T.
struct OofTransform
{
    std::array<std::array<double, 3>, 3> R{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    std::array<double, 3> T{0, 0, 0};

    [[nodiscard]] auto Apply(const std::array<double, 3> &p) const -> std::array<double, 3>;
    /// Directions and normals: the rotation only.
    [[nodiscard]] auto Rotate(const std::array<double, 3> &v) const -> std::array<double, 3>;
    /// this after other: (this * other)(p) = this(other(p)).
    [[nodiscard]] auto Compose(const OofTransform &other) const -> OofTransform;
};

using OofMatrix = std::array<std::array<double, 3>, 3>;

/// The rotation by an engine angle (65536 units per turn, only the low 16 bits count) about an axis (normalized here; a zero
/// axis gives the identity, ok is set to false when the angle is not zero).
[[nodiscard]] auto OofAxisAngle(const std::array<double, 3> &axis, std::int32_t angle, bool *ok = nullptr) -> OofMatrix;

/// Blends two rotations the way the game does: the nine components are interpolated, then the result is rebuilt as
/// Ry(heading) Rx(pitch) Rz(bank). degenerate is set when the blend is not close to a rotation (a half turn apart).
[[nodiscard]] auto OofBlendRotation(const OofMatrix &a, const OofMatrix &b, double f, bool *degenerate = nullptr) -> OofMatrix;

struct OofPoseOptions
{
    /// The unanimated geometry pose: identity rotations and no displacement (what the model looks like without any animation
    /// input). Otherwise the tracks are sampled at U.
    bool Rest{true};
    double U{0};       // normalized animation time, 0 to 1
    double Seconds{0}; // elapsed time, for $rotate
};

struct OofPose
{
    std::vector<OofTransform> Local; // per subobject: p_parent = R p + offset + displacement
    std::vector<OofTransform> World; // per subobject, to the coordinates of the model (parents included)
    std::vector<std::string> Notes;  // problems found while evaluating (degenerate blends, unusable tracks...)
};

/// Evaluates the subobject hierarchy: SOBJ offsets plus PANI displacements, RANI rotations (timed tracks of versions 22 and 23,
/// the uniform keys of the older ones), the $rotate and $fov subobjects. Never throws; the results are finite.
[[nodiscard]] auto EvaluateOofPose(const OofModel &model, const OofPoseOptions &options) -> OofPose;

/// Does the model have anything to animate (tracks with keys, $rotate, $fov)?
[[nodiscard]] auto HasOofAnimation(const OofModel &model) -> bool;
} // namespace Dup::Core
