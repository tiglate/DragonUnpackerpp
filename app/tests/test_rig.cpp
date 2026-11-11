// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/Json.h"
#include "Core/RigModel.h"
#include "Core/RigRender.h"
#include "Dudi7/Driver.h"
#include "TestSupport.h"

#include <clocale>

using namespace Dup;
using namespace Dup::Core;

namespace
{
/// The glTF the driver makes of the skeletal model fixture (two limbs: limb 0 turned 90 degrees about z at (10, 0, 0),
/// limb 1 one unit along x in its frame), as text.
auto FixtureGltf(const char *file = "req_skel.mdl", const char *entry = "req_skel.gltf") -> std::string
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    const auto archive = driver->Open(Tests::Fixture(file));
    for (std::int64_t i = 0; i < archive->GetEntryCount(); ++i)
    {
        if (archive->GetEntry(i).Name == entry)
        {
            const auto bytes = archive->ExtractToMemory(i, 1U << 20);
            return std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        }
    }
    FAIL("the fixture has no glTF entry");
    return {};
}

void CheckFixtureScene(const RigScene &scene)
{
    REQUIRE(scene.Parent.size() == 2);
    CHECK(scene.Parent[0] == -1);
    CHECK(scene.Parent[1] == 0);
    REQUIRE(scene.Animations.size() == 1);
    CHECK(scene.Indices.size() == 6); // two triangles
    const auto pose = EvaluateRig(scene, -1, 0);
    CHECK(pose[0].O[0] == doctest::Approx(10.0));
    CHECK(pose[1].O[0] == doctest::Approx(10.0));
    CHECK(pose[1].O[1] == doctest::Approx(1.0));
    // the vertices (i, i * i, 0) of the file: vertex 3 belongs to limb 1 and ends at (1, 4, 0)
    bool found = false;
    for (std::size_t v = 0; v < scene.JointOf.size(); ++v)
    {
        if (scene.JointOf[v] != 1 || scene.Positions[v * 3] != 3.0f || scene.Positions[v * 3 + 1] != 9.0f)
        {
            continue;
        }
        const auto &j = pose[1];
        const double x = j.R[0] * 3 + j.R[1] * 9 + j.O[0];
        const double y = j.R[3] * 3 + j.R[4] * 9 + j.O[1];
        CHECK(x == doctest::Approx(1.0).epsilon(1e-5));
        CHECK(y == doctest::Approx(4.0).epsilon(1e-5));
        found = true;
    }
    CHECK(found);
}
} // namespace

TEST_CASE("JSON: values, escapes, numbers, errors")
{
    const auto value = ParseJson(R"({"a":[1,-2.5e1,true,null,"xé\n"],"b":{"c":0.125}})");
    REQUIRE(value.has_value());
    const auto *a = value->Find("a");
    REQUIRE(a != nullptr);
    REQUIRE(a->Items.size() == 5);
    CHECK(a->Items[0].Number == 1.0);
    CHECK(a->Items[1].Number == -25.0);
    CHECK(a->Items[2].Boolean);
    CHECK(a->Items[4].Text == "x\xc3\xa9\n");
    CHECK(value->Find("b")->Find("c")->Number == 0.125);
    CHECK_FALSE(ParseJson("{\"a\":}").has_value());
    CHECK_FALSE(ParseJson("[1,2").has_value());
    CHECK_FALSE(ParseJson("[1] x").has_value());
    CHECK_FALSE(ParseJson(std::string(200, '[')).has_value()); // nested too deep
}

TEST_CASE("JSON: numbers do not depend on the locale of the program")
{
    const char *previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = previous != nullptr ? previous : "C";
    bool changed = false;
    for (const char *name : {"pt_BR.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8", "pt_BR", "de_DE"})
    {
        if (std::setlocale(LC_NUMERIC, name) != nullptr)
        {
            changed = true;
            break;
        }
    }
    const auto value = ParseJson("[0.5, 3.88104153, -1.25e-2]");
    REQUIRE(value.has_value());
    CHECK(value->Items[0].Number == 0.5);
    CHECK(value->Items[1].Number == doctest::Approx(3.88104153));
    CHECK(value->Items[2].Number == doctest::Approx(-0.0125));
    if (changed)
    {
        // the driver is loaded into programs like this one: what it writes must not follow the locale either
        const auto scene = ParseRigGltf(FixtureGltf());
        CHECK(scene.has_value());
    }
    std::setlocale(LC_NUMERIC, saved.c_str());
}

TEST_CASE("Rig: the glTF of the skeletal model fixture is read and posed")
{
    const auto scene = ParseRigGltf(FixtureGltf());
    REQUIRE(scene.has_value());
    CheckFixtureScene(*scene);
}

TEST_CASE("Rig: the animation is sampled at its keys, constant tracks stay")
{
    const auto scene = ParseRigGltf(FixtureGltf());
    REQUIRE(scene.has_value());
    const auto at0 = EvaluateRig(*scene, 0, 0.0);
    const auto later = EvaluateRig(*scene, 0, 100.0); // clamped to the last key
    CHECK(at0[1].O[0] == doctest::Approx(10.0));
    CHECK(later[1].O[1] == doctest::Approx(1.0));
}

TEST_CASE("Rig: a texture is drawn over the model, a missing one gives gray, a wireframe is lines")
{
    const auto scene = ParseRigGltf(FixtureGltf());
    REQUIRE(scene.has_value());
    const auto pose = EvaluateRig(*scene, 0, 0.0);
    DecodedImage red;
    red.Width = 1;
    red.Height = 1;
    red.Rgba = {255, 0, 0, 255};
    RigCamera camera;
    const auto count = [](const DecodedImage &image, int r, int g, int b) {
        std::size_t n = 0;
        for (std::size_t i = 0; i + 3 < image.Rgba.size(); i += 4)
        {
            n += (image.Rgba[i] > r && image.Rgba[i + 1] < g && image.Rgba[i + 2] < b) ? 1 : 0;
        }
        return n;
    };
    const auto textured = RenderRig(*scene, pose, camera, 200, 200, {0, 0, 0}, &red);
    const auto gray = RenderRig(*scene, pose, camera, 200, 200, {0, 0, 0}, nullptr);
    CHECK(count(textured, 60, 20, 20) > 100); // red
    CHECK(count(gray, 60, 20, 20) == 0);
    camera.Wireframe = true;
    const auto lines = RenderRig(*scene, pose, camera, 200, 200, {0, 0, 0}, nullptr);
    std::size_t lit = 0;
    for (std::size_t i = 0; i < lines.Rgba.size(); i += 4)
    {
        lit += lines.Rgba[i] > 0 ? 1 : 0;
    }
    CHECK(lit > 20);
}

TEST_CASE("Rig: a level has one part per texture, each drawn with its own picture")
{
    // the level of the .jam fixture: two pieces with art slot 7, a triangle with slot 9
    const auto scene = ParseRigGltf(FixtureGltf("req_v2.jam", "level.gltf"));
    REQUIRE(scene.has_value());
    REQUIRE(scene->Parts.size() == 2);
    CHECK(scene->Parts[0].ArtSlot == 7);
    CHECK(scene->Parts[1].ArtSlot == 9);
    CHECK(scene->Parts[0].UvSize == doctest::Approx(128.0));
    CHECK(scene->Parts[0].IndexCount == 6);
    CHECK(scene->Parts[1].FirstIndex == 6);
    CHECK(scene->Parts[1].IndexCount == 3);
    const auto pose = EvaluateRig(*scene, -1, 0.0);
    DecodedImage red;
    red.Width = red.Height = 1;
    red.Rgba = {255, 0, 0, 255};
    DecodedImage blue;
    blue.Width = blue.Height = 1;
    blue.Rgba = {0, 0, 255, 255};
    RigCamera camera;
    camera.Pitch = 0.6;
    const auto image = RenderRig(*scene, pose, camera, 200, 200, {0, 0, 0}, std::vector<const DecodedImage *>{&red, &blue});
    std::size_t reds = 0;
    std::size_t blues = 0;
    for (std::size_t i = 0; i + 3 < image.Rgba.size(); i += 4)
    {
        reds += image.Rgba[i] > 60 && image.Rgba[i + 2] < 20 ? 1 : 0;
        blues += image.Rgba[i + 2] > 60 && image.Rgba[i] < 20 ? 1 : 0;
    }
    CHECK(reds > 100);
    CHECK(blues > 100);
}

TEST_CASE("Rig: files that are not a skinned model are refused")
{
    CHECK_FALSE(ParseRigGltf("not json").has_value());
    CHECK_FALSE(ParseRigGltf("{}").has_value());
    CHECK_FALSE(ParseRigGltf(R"({"buffers":[{"uri":"x.bin"}],"nodes":[],"skins":[],"meshes":[]})").has_value());
}
