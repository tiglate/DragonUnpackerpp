// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The worked checks of "Assembling and displaying an OOF" in the Descent 3 format specification.

#include "Core/OofPose.h"

#include "ThirdParty/doctest.h"

#include <cmath>

using namespace Dup::Core;

namespace
{
constexpr std::int32_t QUARTER = 16384; // 65536 units per turn

auto Node(std::int32_t parent, OofVec3 offset) -> OofSubobject
{
    OofSubobject so;
    so.Parent = parent;
    so.Offset = offset;
    return so;
}

auto TwoNodes(std::size_t version = 2200) -> OofModel
{
    OofModel model;
    model.RawVersion = model.Version = static_cast<std::int32_t>(version);
    model.Subobjects = {Node(-1, {10, 0, 0}), Node(0, {0, 2, 0})};
    model.Rotation.resize(2);
    model.Position.resize(2);
    return model;
}

void HoldRotation(OofTrack<OofRotationKey> &track, OofVec3 axis, std::int32_t angle)
{
    track.Keys = {{0, axis, angle}}; // untimed, one key: held
}

void Near(const std::array<double, 3> &p, double x, double y, double z)
{
    CHECK(p[0] == doctest::Approx(x).epsilon(1e-9));
    CHECK(p[1] == doctest::Approx(y).epsilon(1e-9));
    CHECK(p[2] == doctest::Approx(z).epsilon(1e-9));
}
} // namespace

TEST_CASE("Pose: offsets of root and child in the unanimated pose")
{
    const auto model = TwoNodes();
    const auto pose = EvaluateOofPose(model, {});
    Near(pose.World[1].Apply({1, 0, 0}), 11, 2, 0);
}

TEST_CASE("Pose: rotations act about the local origin, translations are in the parent axes")
{
    auto model = TwoNodes(2100); // untimed profile: one key holds
    HoldRotation(model.Rotation[0], {0, 0, 1}, QUARTER);
    OofPoseOptions options;
    options.Rest = false;
    Near(EvaluateOofPose(model, options).World[1].Apply({1, 0, 0}), 8, 1, 0);
    HoldRotation(model.Rotation[1], {0, 0, 1}, QUARTER);
    Near(EvaluateOofPose(model, options).World[1].Apply({1, 0, 0}), 7, 0, 0);
    model.Position[1].Keys = {{0, {0, 1, 0}}};
    Near(EvaluateOofPose(model, options).World[1].Apply({1, 0, 0}), 6, 0, 0);
    // the rest pose ignores every track
    Near(EvaluateOofPose(model, {}).World[1].Apply({1, 0, 0}), 11, 2, 0);
}

TEST_CASE("Pose: the geometric center does not move anything")
{
    auto model = TwoNodes();
    model.Subobjects[1].GeometricCenter = {4, 0, 0};
    Near(EvaluateOofPose(model, {}).World[1].Apply({1, 0, 0}), 11, 2, 0);
}

TEST_CASE("Pose: keys accumulate, the later rotation applies last")
{
    auto model = TwoNodes(2100);
    model.Rotation[0].Keys = {{0, {0, 0, 1}, QUARTER}, {1, {1, 0, 0}, QUARTER}};
    OofPoseOptions options;
    options.Rest = false;
    options.U = 1.0;
    Near(EvaluateOofPose(model, options).Local[0].Rotate({1, 0, 0}), 0, 0, 1);
}

TEST_CASE("Pose: timed position track")
{
    auto model = TwoNodes(2200);
    auto &track = model.Position[1];
    track.Timed = true;
    track.MinTick = 10;
    track.MaxTick = 30;
    track.Keys = {{10, {0, 0, 0}}, {20, {0, 4, 0}}, {30, {0, 4, 2}}};
    OofPoseOptions options;
    options.Rest = false;
    options.U = 0.25;
    Near(EvaluateOofPose(model, options).Local[1].T, 0, 4, 0); // offset (0,2,0) + displacement (0,2,0)
    options.U = 1.0;
    Near(EvaluateOofPose(model, options).Local[1].T, 0, 6, 2); // offset + the last key
}

TEST_CASE("Pose: timed tracks with one key give nothing, untimed ones hold it; the untimed first interval uses only the second key")
{
    auto timed = TwoNodes(2200);
    timed.Position[1].Timed = true;
    timed.Position[1].Keys = {{0, {0, 9, 0}}};
    OofPoseOptions options;
    options.Rest = false;
    Near(EvaluateOofPose(timed, options).Local[1].T, 0, 2, 0);
    auto untimed = TwoNodes(2100);
    untimed.Position[1].Keys = {{0, {5, 0, 0}}, {1, {0, 2, 0}}};
    options.U = 0.25; // first interval, fraction 0.25
    Near(EvaluateOofPose(untimed, options).Local[1].T, 0, 2.5, 0);
    options.U = 0.75; // the only interval is the first one
    Near(EvaluateOofPose(untimed, options).Local[1].T, 0, 3.5, 0);
}

TEST_CASE("Pose: the blend of two rotations is rebuilt, not applied raw")
{
    const auto half = OofBlendRotation(OofAxisAngle({0, 0, 1}, 0), OofAxisAngle({0, 0, 1}, QUARTER), 0.5);
    const std::array<double, 3> v{1, 0, 0};
    const std::array<double, 3> r{half[0][0] * v[0], half[1][0] * v[0], half[2][0] * v[0]};
    Near(r, std::sqrt(0.5), std::sqrt(0.5), 0);
}

TEST_CASE("Pose: rotation signs")
{
    const auto z = OofAxisAngle({0, 0, 1}, QUARTER);
    CHECK(z[1][0] == doctest::Approx(1.0)); // (1,0,0) goes to (0,1,0)
    const auto y = OofAxisAngle({0, 1, 0}, QUARTER);
    CHECK(y[0][2] == doctest::Approx(1.0)); // (0,0,1) goes to (1,0,0)
    bool ok = true;
    (void)OofAxisAngle({0, 0, 0}, 5, &ok);
    CHECK_FALSE(ok);
    (void)OofAxisAngle({0, 0, 0}, 0, &ok);
    CHECK(ok);
}

TEST_CASE("Pose: $rotate turns about the axis of the second key")
{
    auto model = TwoNodes(2100);
    model.Subobjects[1].Properties = "$rotate=4";
    model.Rotation[1].Keys = {{0, {1, 0, 0}, 0}, {1, {0, 0, 1}, 0}};
    OofPoseOptions options;
    options.Rest = false;
    options.Seconds = 1.0; // a quarter of the 4 seconds
    const auto pose = EvaluateOofPose(model, options);
    const auto r = pose.Local[1].Rotate({1, 0, 0});
    CHECK(r[0] == doctest::Approx(0.0).epsilon(1e-3));
    CHECK(r[1] == doctest::Approx(1.0).epsilon(1e-3));
    CHECK(HasOofAnimation(model));
}

TEST_CASE("Pose: damaged tracks do not give NaN")
{
    auto model = TwoNodes(2200);
    model.Rotation[0].Timed = true;
    model.Rotation[0].MinTick = 5;
    model.Rotation[0].MaxTick = 5; // empty range
    model.Rotation[0].Keys = {{5, {0, 0, 0}, QUARTER}, {5, {0, 0, 0}, 0}};
    OofPoseOptions options;
    options.Rest = false;
    for (const double u : {0.0, 0.5, 1.0})
    {
        options.U = u;
        const auto pose = EvaluateOofPose(model, options);
        for (const auto &t : pose.World)
        {
            for (const auto &row : t.R)
            {
                for (const double c : row)
                {
                    CHECK(std::isfinite(c));
                }
            }
        }
    }
}
