// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/OofRender.h"

#include "ThirdParty/doctest.h"

using namespace Dup::Core;

namespace
{
/// A 2x2 picture: red, green on the top row, blue, white on the bottom row.
auto Picture() -> std::shared_ptr<const DecodedImage>
{
    auto image = std::make_shared<DecodedImage>();
    image->Width = 2;
    image->Height = 2;
    image->Rgba = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    return image;
}

/// One square facing -Z (towards the default camera) with the picture mapped over it, (0,0) at its top left.
auto Square(float normalZ) -> std::shared_ptr<const OofModel>
{
    auto model = std::make_shared<OofModel>();
    model->Version = 2200;
    OofSubobject so;
    so.Positions = {{-1, 1, 0}, {1, 1, 0}, {1, -1, 0}, {-1, -1, 0}};
    so.Normals.assign(4, {0, 0, normalZ});
    OofFace face;
    face.Normal = {0, 0, normalZ};
    face.Texture = 0;
    face.Corners = {{0, 0, 0}, {1, 1, 0}, {2, 1, 1}, {3, 0, 1}};
    so.Faces = {face};
    model->Subobjects = {so};
    model->Textures = {"pic"};
    return model;
}

auto Pixel(const DecodedImage &image, std::uint32_t x, std::uint32_t y) -> std::array<int, 3>
{
    const auto *p = &image.Rgba[(static_cast<std::size_t>(y) * image.Width + x) * 4];
    return {p[0], p[1], p[2]};
}

auto Render(const std::shared_ptr<const OofModel> &model, bool cull = true) -> DecodedImage
{
    const auto scene = MakeOofScene(model, [](const std::string &name) { return name == "pic" ? Picture() : nullptr; });
    OofCamera camera;
    camera.Yaw = 0;
    camera.Pitch = 0;
    camera.CullBackFaces = cull;
    return RenderOof(*scene, EvaluateOofPose(*model, {}), camera, 100, 100, {0, 0, 0});
}
} // namespace

TEST_CASE("OOF render: V grows downwards, U to the right, as the stored UVs say")
{
    const auto image = Render(Square(-1.0f));
    // brightness changes the colours, so only the dominant channel is compared
    auto dominant = [&](std::uint32_t x, std::uint32_t y) {
        const auto p = Pixel(image, x, y);
        return p[0] > 100 && p[1] < 60 && p[2] < 60 ? 'r' : p[1] > 100 && p[0] < 60 && p[2] < 60 ? 'g' : p[2] > 100 && p[0] < 60 && p[1] < 60 ? 'b' : p[0] > 100 && p[1] > 100 && p[2] > 100 ? 'w' : '?';
    };
    CHECK(dominant(35, 35) == 'r'); // top left
    CHECK(dominant(65, 35) == 'g'); // top right
    CHECK(dominant(35, 65) == 'b'); // bottom left
    CHECK(dominant(65, 65) == 'w'); // bottom right
}

TEST_CASE("OOF render: a face turned away is not drawn, unless back faces are asked for")
{
    CHECK(Pixel(Render(Square(1.0f)), 50, 50) == std::array<int, 3>{0, 0, 0});
    CHECK(Pixel(Render(Square(1.0f), false), 50, 50) != std::array<int, 3>{0, 0, 0});
}

TEST_CASE("OOF render: a texture that was not found is drawn as a diagnostic checkerboard")
{
    const auto model = Square(-1.0f);
    const auto scene = MakeOofScene(model, nullptr);
    CHECK(scene->MissingTextures == 1);
    OofCamera camera;
    camera.Yaw = camera.Pitch = 0;
    const auto image = RenderOof(*scene, EvaluateOofPose(*model, {}), camera, 100, 100, {0, 0, 0});
    CHECK(Pixel(image, 50, 50) != std::array<int, 3>{0, 0, 0});
}

TEST_CASE("OOF render: the scene counts triangles and finds the animation")
{
    const auto scene = MakeOofScene(Square(-1.0f), nullptr);
    CHECK(scene->Triangles == 2);
    CHECK_FALSE(scene->Animated);
}
