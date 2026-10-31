// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ObjParser.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Dup::Core
{
using OofVec3 = std::array<float, 3>;

struct OofCorner
{
    std::int32_t Vertex{0};
    float U{0}, V{0};
};

struct OofFace
{
    OofVec3 Normal{};
    std::int32_t Texture{-1}; // index into OofModel::Textures, -1 for a flat coloured face
    std::array<std::uint8_t, 3> Color{};
    std::vector<OofCorner> Corners;
};

struct OofSubobject
{
    std::int32_t Parent{-1};
    OofVec3 PlaneNormal{}, PlanePoint{}, Offset{}, GeometricCenter{};
    float PlaneDistance{0}, Radius{0};
    std::int32_t MovementType{0}, MovementAxis{0};
    std::string Name, Properties; // the properties are text ("$rotate=4", "$glow=1,1,1,2"...)
    std::vector<OofVec3> Positions, Normals;
    std::vector<float> Alpha; // empty before version 23 (1.0 everywhere)
    std::vector<OofFace> Faces;
};

struct OofPoint
{
    std::int32_t Parent{0};
    OofVec3 Position{}, Normal{};
};

struct OofBattery
{
    std::vector<std::int32_t> Gunpoints, Turrets;
};

struct OofRotationKey
{
    std::int32_t Tick{0};
    OofVec3 Axis{};
    std::int32_t Angle{0}; // 65536 units per turn
};

struct OofPositionKey
{
    std::int32_t Tick{0};
    OofVec3 Position{};
};

/// A track of RANI/ANIM or PANI for one subobject. Timed tracks (major 22 and 23) have their own tick range; the keys of the
/// older profile (major 18 to 21) have no times: they are spread over the whole animation and Timed is false.
template <typename Key> struct OofTrack
{
    bool Timed{false};
    std::int32_t MinTick{0}, MaxTick{0};
    std::vector<Key> Keys;
};

struct OofModel
{
    std::int32_t RawVersion{0};
    std::int32_t Version{0}; // normalized: below 18 it is multiplied by 100
    float Radius{0};
    OofVec3 BoxMin{}, BoxMax{};
    std::vector<std::int32_t> Details;
    std::vector<OofSubobject> Subobjects;
    std::vector<std::string> Textures;
    std::vector<OofPoint> Gunpoints, Attachments, GroundContacts;
    std::vector<OofVec3> AttachmentUp; // from NATH, in the order of Attachments
    std::vector<OofBattery> Batteries;
    std::vector<OofTrack<OofRotationKey>> Rotation; // per subobject, from RANI/ANIM (empty without the chunk)
    std::vector<OofTrack<OofPositionKey>> Position; // per subobject, from PANI
    std::vector<std::string> Notes;                     // things the game would limit or ignore
};

/// What the property text of a subobject says (matching is case-insensitive; unknown text is ignored).
struct OofProperties
{
    bool Jitter{false}, Shell{false}, Facing{false}, FrontFace{false}, Viewer{false}, Layer{false}, Custom{false};
    int Monitor{0};                    // 1 to 8 for $monitor01 to $monitor08, 0 for none
    std::optional<double> RotateSeconds; // $rotate=: seconds per revolution (valid when above 0 and at most 20)
    std::optional<std::array<double, 3>> Fov; // $fov=: degrees, seconds per revolution, reaction time
    std::optional<std::array<double, 4>> Glow, Thruster; // red, green, blue, size
};

[[nodiscard]] auto ParseOofProperties(const std::string &text) -> OofProperties;

/// The subobjects whose authored polygons the game draws (see MakeOofMesh): no door helpers, effect subobjects or faceless
/// nodes, and not the children of the helpers and faceless nodes when pruneSubtrees is set.
[[nodiscard]] auto OofDrawnSubobjects(const OofModel &model, bool pruneSubtrees = true) -> std::vector<bool>;

/// Does it start with "PSPO"?
[[nodiscard]] auto IsOof(std::span<const std::byte> data) -> bool;

/// Parses a Descent 3 object model (versions 18.07 to 23.00) and checks it: nullopt with the reason in error when it is
/// malformed (a chunk that runs past the file, an index out of range, a parent cycle, non-finite numbers...).
[[nodiscard]] auto ParseOof(std::span<const std::byte> data, std::string *error = nullptr) -> std::optional<OofModel>;

/// Wavefront OBJ text of every subobject (one object each, placed by the offsets of its ancestors in the unanimated pose), faces
/// with their texture names as materials. The game is left-handed with +Z forward and V counted from the top: Z is negated, the
/// winding reversed and V flipped so that the OBJ is not mirrored.
[[nodiscard]] auto ExportOofObj(const OofModel &model) -> std::string;

/// Triangles for the 3D preview in the same right-handed coordinates: what the game draws as ordinary polygons (no door helpers,
/// effect subobjects or faceless nodes; the OHDR detail list is not used). With pruneSubtrees the children of the helpers and
/// faceless nodes are left out too, as the game does.
[[nodiscard]] auto MakeOofMesh(const OofModel &model, bool pruneSubtrees = true, std::size_t maxTriangles = 400000) -> std::optional<Mesh>;

/// One line summary for the preview ("12 subobjects, 840 faces; textures: a, b").
[[nodiscard]] auto DescribeOof(const OofModel &model) -> std::string;
} // namespace Dup::Core
