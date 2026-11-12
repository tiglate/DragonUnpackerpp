// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The pose of a Descent 3 model, from the "Assembling and displaying an OOF" section of the format specification.

#include "Core/OofPose.h"

#include <algorithm>
#include <cmath>

namespace Dup::Core
{
namespace
{
using Vec = std::array<double, 3>;
constexpr double PI = 3.14159265358979323846;

auto Multiply(const OofMatrix &a, const OofMatrix &b) -> OofMatrix
{
    OofMatrix r{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            for (int k = 0; k < 3; ++k)
            {
                r[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] += a[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] * b[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)];
            }
        }
    }
    return r;
}

auto Identity() -> OofMatrix
{
    return {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
}

auto Finite(const OofMatrix &m) -> bool
{
    for (const auto &row : m)
    {
        for (const double v : row)
        {
            if (!std::isfinite(v))
            {
                return false;
            }
        }
    }
    return true;
}

auto RotX(double a) -> OofMatrix
{
    const double c = std::cos(a), s = std::sin(a);
    return {{{1, 0, 0}, {0, c, -s}, {0, s, c}}};
}
auto RotY(double a) -> OofMatrix
{
    const double c = std::cos(a), s = std::sin(a);
    return {{{c, 0, s}, {0, 1, 0}, {-s, 0, c}}};
}
auto RotZ(double a) -> OofMatrix
{
    const double c = std::cos(a), s = std::sin(a);
    return {{{c, -s, 0}, {s, c, 0}, {0, 0, 1}}};
}

auto Lerp(const Vec &a, const Vec &b, double f) -> Vec
{
    return {a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f};
}

auto Axis(const OofVec3 &v) -> Vec
{
    return {static_cast<double>(v[0]), static_cast<double>(v[1]), static_cast<double>(v[2])};
}

/// A key position for a track: which key, and how far to the next one.
struct KeyPlace
{
    std::size_t Key{0};
    double Fraction{0};
};

/// Timed tracks: the last key that starts no later than the tick, and the fraction towards the next one.
auto TimedPlace(std::int32_t minTick, std::int32_t maxTick, const std::vector<std::int32_t> &ticks, double u, std::vector<std::string> &notes) -> KeyPlace
{
    const std::size_t last = ticks.size() - 1;
    if (u >= 1.0)
    {
        return {last, 0.0}; // the last key, without a remap lookup at the exclusive maximum
    }
    const double tau = minTick + u * (static_cast<double>(maxTick) - minTick);
    const auto whole = static_cast<std::int64_t>(std::trunc(tau));
    std::size_t k = 0;
    bool found = false;
    for (std::size_t i = 0; i < ticks.size(); ++i)
    {
        if (ticks[i] <= whole)
        {
            k = i;
            found = true;
        }
    }
    if (!found)
    {
        notes.push_back("the animation starts after its first key");
        return {0, 0.0};
    }
    if (k == last)
    {
        return {k, 0.0};
    }
    const double span = static_cast<double>(ticks[k + 1]) - ticks[k];
    if (span <= 0)
    {
        notes.push_back("animation key times are not increasing");
        return {k, 0.0};
    }
    return {k, std::clamp((tau - ticks[k]) / span, 0.0, 1.0)};
}

/// Untimed tracks: the keys are spread uniformly over the animation.
auto UniformPlace(std::size_t count, double u) -> KeyPlace
{
    const std::size_t last = count - 1;
    if (u >= 1.0)
    {
        return {last, 0.0};
    }
    const double scaled = u * static_cast<double>(last);
    const auto k = std::min(static_cast<std::size_t>(std::trunc(scaled)), last);
    return {k, k == last ? 0.0 : scaled - static_cast<double>(k)};
}

/// The accumulated rotation of every key: A0 = D0, Ak = Dk Ak-1.
auto Accumulate(const std::vector<OofRotationKey> &keys, std::vector<std::string> &notes) -> std::vector<OofMatrix>
{
    std::vector<OofMatrix> accumulated;
    OofMatrix current = Identity();
    for (const auto &key : keys)
    {
        bool ok = true;
        const auto delta = OofAxisAngle(Axis(key.Axis), key.Angle, &ok);
        if (!ok)
        {
            notes.push_back("a rotation key has an angle but no axis");
        }
        current = Multiply(delta, current);
        accumulated.push_back(current);
    }
    return accumulated;
}

auto EvaluateRotation(const OofTrack<OofRotationKey> &track, double u, std::vector<std::string> &notes) -> OofMatrix
{
    if (track.Keys.empty())
    {
        return Identity();
    }
    if (track.Keys.size() == 1)
    {
        // timed: identity; untimed: the sole accumulated rotation is held
        return track.Timed ? Identity() : Accumulate(track.Keys, notes)[0];
    }
    const auto accumulated = Accumulate(track.Keys, notes);
    KeyPlace place;
    if (track.Timed)
    {
        std::vector<std::int32_t> ticks;
        for (const auto &key : track.Keys)
        {
            ticks.push_back(key.Tick);
        }
        place = TimedPlace(track.MinTick, track.MaxTick, ticks, u, notes);
    }
    else
    {
        place = UniformPlace(track.Keys.size(), u);
    }
    if (place.Fraction <= 0.0 || place.Key + 1 >= accumulated.size())
    {
        return accumulated[place.Key];
    }
    bool degenerate = false;
    const auto result = OofBlendRotation(accumulated[place.Key], accumulated[place.Key + 1], place.Fraction, &degenerate);
    if (degenerate)
    {
        notes.push_back("a rotation blend is degenerate (keys half a turn apart)");
    }
    return result;
}

auto EvaluateDisplacement(const OofTrack<OofPositionKey> &track, double u, std::vector<std::string> &notes) -> Vec
{
    const auto &keys = track.Keys;
    if (keys.empty())
    {
        return {0, 0, 0};
    }
    if (keys.size() == 1)
    {
        return track.Timed ? Vec{0, 0, 0} : Axis(keys[0].Position); // untimed: the sole position is held
    }
    KeyPlace place;
    if (track.Timed)
    {
        std::vector<std::int32_t> ticks;
        for (const auto &key : keys)
        {
            ticks.push_back(key.Tick);
        }
        place = TimedPlace(track.MinTick, track.MaxTick, ticks, u, notes);
    }
    else
    {
        place = UniformPlace(keys.size(), u);
    }
    const Vec current = Axis(keys[place.Key].Position);
    if (place.Key + 1 >= keys.size())
    {
        return current;
    }
    const Vec next = Axis(keys[place.Key + 1].Position);
    if (!track.Timed && place.Key == 0)
    {
        // compatibility quirk of the untimed profile: the first interval ignores the first position
        return {next[0] * place.Fraction, next[1] * place.Fraction, next[2] * place.Fraction};
    }
    return Lerp(current, next, place.Fraction);
}
} // namespace

auto OofTransform::Apply(const std::array<double, 3> &p) const -> std::array<double, 3>
{
    auto r = Rotate(p);
    return {r[0] + T[0], r[1] + T[1], r[2] + T[2]};
}

auto OofTransform::Rotate(const std::array<double, 3> &v) const -> std::array<double, 3>
{
    return {R[0][0] * v[0] + R[0][1] * v[1] + R[0][2] * v[2], R[1][0] * v[0] + R[1][1] * v[1] + R[1][2] * v[2], R[2][0] * v[0] + R[2][1] * v[1] + R[2][2] * v[2]};
}

auto OofTransform::Compose(const OofTransform &other) const -> OofTransform
{
    OofTransform result;
    result.R = Multiply(R, other.R);
    result.T = Apply(other.T);
    return result;
}

auto OofAxisAngle(const std::array<double, 3> &axis, std::int32_t angle, bool *ok) -> OofMatrix
{
    if (ok != nullptr)
    {
        *ok = true;
    }
    const double length = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    const auto units = static_cast<std::uint32_t>(angle) & 0xFFFFU;
    if (!(length > 1e-12) || !std::isfinite(length))
    {
        if (ok != nullptr)
        {
            *ok = units == 0;
        }
        return Identity();
    }
    const double a[3] = {axis[0] / length, axis[1] / length, axis[2] / length};
    const double theta = 2.0 * PI * units / 65536.0;
    const double c = std::cos(theta), s = std::sin(theta), t = 1.0 - c;
    // D v = cos v + sin (a x v) + (1 - cos) a (a . v)
    return {{{c + t * a[0] * a[0], t * a[0] * a[1] - s * a[2], t * a[0] * a[2] + s * a[1]},
             {t * a[0] * a[1] + s * a[2], c + t * a[1] * a[1], t * a[1] * a[2] - s * a[0]},
             {t * a[0] * a[2] - s * a[1], t * a[1] * a[2] + s * a[0], c + t * a[2] * a[2]}}};
}

auto OofBlendRotation(const OofMatrix &a, const OofMatrix &b, double f, bool *degenerate) -> OofMatrix
{
    OofMatrix l;
    for (std::size_t i = 0; i < 3; ++i)
    {
        for (std::size_t j = 0; j < 3; ++j)
        {
            l[i][j] = (1.0 - f) * a[i][j] + f * b[i][j];
        }
    }
    // the columns are the right, up and forward vectors
    const double rx = l[0][0], ry = l[1][0], rz = l[2][0];
    const double uy = l[1][1];
    const double vx = l[0][2], vy = l[1][2], vz = l[2][2];
    if (degenerate != nullptr)
    {
        const double lengthF = std::sqrt(vx * vx + vy * vy + vz * vz);
        *degenerate = lengthF < 0.05;
    }
    double h, p, bank;
    if (std::abs(vx) < 0.00001 && std::abs(vz) < 0.00001)
    {
        p = vy > 0 ? -PI / 2 : PI / 2;
        bank = 0;
        h = std::atan2(-rz, rx);
    }
    else
    {
        h = std::atan2(vx, vz);
        const double ch = std::cos(h), sh = std::sin(h);
        const double c = std::abs(sh) > std::abs(ch) ? vx / sh : vz / ch;
        p = std::atan2(-vy, c);
        bank = std::atan2(ry / c, uy / c);
    }
    const auto result = Multiply(Multiply(RotY(h), RotX(p)), RotZ(bank));
    if (!Finite(result))
    {
        if (degenerate != nullptr)
        {
            *degenerate = true;
        }
        return Identity();
    }
    return result;
}

auto HasOofAnimation(const OofModel &model) -> bool
{
    for (const auto &track : model.Rotation)
    {
        if (track.Keys.size() > 1)
        {
            return true;
        }
    }
    for (const auto &track : model.Position)
    {
        if (track.Keys.size() > 1)
        {
            return true;
        }
    }
    for (const auto &so : model.Subobjects)
    {
        const auto properties = ParseOofProperties(so.Properties);
        if (properties.RotateSeconds || properties.Fov)
        {
            return true;
        }
    }
    return false;
}

auto EvaluateOofPose(const OofModel &model, const OofPoseOptions &options) -> OofPose
{
    const std::size_t count = model.Subobjects.size();
    OofPose pose;
    pose.Local.resize(count);
    pose.World.resize(count);
    const double u = std::clamp(std::isfinite(options.U) ? options.U : 0.0, 0.0, 1.0);
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto &so = model.Subobjects[i];
        OofTransform local;
        local.T = Axis(so.Offset);
        if (!options.Rest)
        {
            OofMatrix rotation = Identity();
            if (i < model.Rotation.size())
            {
                rotation = EvaluateRotation(model.Rotation[i], u, pose.Notes);
            }
            if (i < model.Position.size())
            {
                const auto d = EvaluateDisplacement(model.Position[i], u, pose.Notes);
                for (std::size_t c = 0; c < 3; ++c)
                {
                    local.T[c] += d[c];
                }
            }
            // rotators and turrets turn about the axis of the second key, replacing the ordinary rotation
            const auto properties = ParseOofProperties(so.Properties);
            if (properties.RotateSeconds || properties.Fov)
            {
                // the game reads the axis of the second key; with a single key (the game would read past the track) the only axis is used
                const std::size_t keys = i < model.Rotation.size() ? model.Rotation[i].Keys.size() : 0;
                if (keys >= 1)
                {
                    if (keys == 1)
                    {
                        pose.Notes.push_back("a rotator has one rotation key: its axis is used");
                    }
                    const double fraction = properties.RotateSeconds ? std::fmod(options.Seconds / *properties.RotateSeconds, 1.0) : u;
                    bool ok = true;
                    rotation = OofAxisAngle(Axis(model.Rotation[i].Keys[keys >= 2 ? 1 : 0].Axis), static_cast<std::int32_t>(std::trunc(std::max(0.0, fraction) * 65535.0)), &ok);
                    if (!ok)
                    {
                        pose.Notes.push_back("a rotator has no usable axis");
                    }
                }
                else
                {
                    pose.Notes.push_back("a rotator has no rotation keys: it does not turn");
                }
            }
            local.R = rotation;
        }
        pose.Local[i] = local;
    }
    // parents are validated: no cycles; the memo makes every node composed once
    std::vector<bool> done(count, false);
    for (std::size_t i = 0; i < count; ++i)
    {
        std::vector<std::size_t> chain;
        for (auto at = static_cast<std::int64_t>(i); at != -1 && !done[static_cast<std::size_t>(at)];
             at = model.Subobjects[static_cast<std::size_t>(at)].Parent)
        {
            chain.push_back(static_cast<std::size_t>(at));
            if (chain.size() > count)
            {
                break;
            }
        }
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            const auto parent = model.Subobjects[*it].Parent;
            pose.World[*it] = parent < 0 ? pose.Local[*it] : pose.World[static_cast<std::size_t>(parent)].Compose(pose.Local[*it]);
            done[*it] = true;
        }
    }
    std::sort(pose.Notes.begin(), pose.Notes.end());
    pose.Notes.erase(std::unique(pose.Notes.begin(), pose.Notes.end()), pose.Notes.end());
    return pose;
}
} // namespace Dup::Core
