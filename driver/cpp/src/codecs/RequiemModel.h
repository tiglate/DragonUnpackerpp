// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Requiem: Avenging Angel models (.mdl files, Models3.txt, the animations) read, posed and converted to OBJ and
// glTF: shared by the model files and the animation pack, which shows each animation on its model. The layouts
// are in RequiemModel.cpp.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Reader.h"

namespace Dud::RequiemModel
{
struct Vec3
{
    float X = 0;
    float Y = 0;
    float Z = 0;
};

struct Face
{
    std::vector<uint16_t> Index;
    std::vector<std::pair<int32_t, int32_t>> Uv;
};

struct Model
{
    std::string Texture;
    std::vector<Vec3> Vertices;
    std::vector<Face> Faces;
    std::vector<Vec3> Normals;
    bool FaceNormals = false;
    std::vector<uint32_t> LimbVertices; // skeletal models: vertices of each limb
    std::vector<uint32_t> SectionCodes; // skeletal models: first u32 of each limb record (the skeleton)
    std::string PoseSource;             // what posed the model (empty: not posed)
    int ArtSlot = -1;                   // texture slot of Models3.txt
    std::vector<int> OtherSlots;        // the other slots it lists for the same file
};

struct ModelEntry
{
    char Kind = 'M';
    int Id = 0;
    int Slot = -1;
    std::string File;
    std::vector<std::pair<int, std::string>> Animations;
};

struct Animation
{
    std::string Name;
    std::vector<uint8_t> Bytes;
};


// Reads a static or a skeletal model; false unless the file is exactly one
auto ParseStatic(Reader &r, Model &m) -> bool;
auto ParseSkeletal(Reader &r, Model &m) -> bool;

auto ReadFileBytes(const std::string &path) -> std::vector<uint8_t>;
// A file of a folder by its name, whatever the case (empty if there is none)
auto FindInFolder(const std::string &folder, const std::string &name) -> std::string;

// Models3.txt of a folder (see RequiemModel.cpp)
auto ReadModels3(const std::string &folder) -> std::vector<ModelEntry>;
// The texture slots of a model file: the first that is not negative, then the others
void SlotsOf(const std::vector<ModelEntry> &entries, const std::string &file, int &slot, std::vector<int> &others);

// AnimFile.tab: for each model id of the M lines of Models3.txt, AnimationsPerModel u16 numbers of records of
// AnimFile.ary/.bin, one per animation number ("A n file.ani"). Empty if the file cannot be read.
constexpr int AnimationsPerModel = 40;
auto ReadAnimTable(const std::string &path) -> std::vector<uint16_t>;
// The record of an animation of a model in that table (not checked against the count of records)
auto AnimRecordOf(const std::vector<uint16_t> &table, int model, int animation) -> std::optional<size_t>;

// The animations that fit the model, the one that poses it first; its texture slots are set too
auto CollectAnimations(const Model &m, const std::string &path, int &artSlot, std::vector<int> &otherSlots) -> std::vector<Animation>;
// Does the animation file fit a model of that many limbs (its limbs end exactly where the file ends)?
auto AnimationFits(const std::vector<uint8_t> &file, size_t limbs) -> bool;
// The vertices of the model in the first frame of the animation (world positions)
void PoseModel(Model &m, const Animation &animation);

auto MakeObj(const Model &m, const std::string &base) -> std::string;

// The animated glTF of a model. The mesh part is built once and reused for every set of animations.
class GltfBuilder
{
  public:
    GltfBuilder(const Model &model, const std::string &base);
    ~GltfBuilder();
    GltfBuilder(const GltfBuilder &) = delete;
    auto operator=(const GltfBuilder &) -> GltfBuilder & = delete;
    // Empty if the skeleton cannot be built (see RequiemModel.cpp)
    auto Build(const std::vector<Animation> &animations) const -> std::string;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
// A static model (no animations) or a skeletal one with its animations
auto MakeGltf(const Model &m, const std::vector<Animation> &animations, const std::string &base) -> std::string;
} // namespace Dud::RequiemModel
