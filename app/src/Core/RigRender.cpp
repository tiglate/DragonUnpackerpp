// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/RigRender.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Dup::Core
{
namespace
{
struct Vertex
{
    double X{0}; // screen
    double Y{0};
    double Z{0}; // depth: larger is closer to the camera
    double U{0};
    double V{0};
    double LU{0}; // in the lightmap, pixels
    double LV{0};
};

// The lightmap at a point (pixels, sample i at i + 0.5), bilinear and clamped to the picture: what the game's
// hardware renderers do
void SampleLight(const DecodedImage &map, double x, double y, double out[3])
{
    x = std::clamp(x - 0.5, 0.0, static_cast<double>(map.Width - 1));
    y = std::clamp(y - 0.5, 0.0, static_cast<double>(map.Height - 1));
    const auto x0 = static_cast<std::size_t>(x);
    const auto y0 = static_cast<std::size_t>(y);
    const std::size_t x1 = std::min<std::size_t>(x0 + 1, map.Width - 1);
    const std::size_t y1 = std::min<std::size_t>(y0 + 1, map.Height - 1);
    const double fx = x - static_cast<double>(x0);
    const double fy = y - static_cast<double>(y0);
    for (int c = 0; c < 3; ++c)
    {
        const auto at = [&](std::size_t px, std::size_t py) { return static_cast<double>(map.Rgba[(py * map.Width + px) * 4 + static_cast<std::size_t>(c)]); };
        out[c] = ((at(x0, y0) * (1 - fx) + at(x1, y0) * fx) * (1 - fy) + (at(x0, y1) * (1 - fx) + at(x1, y1) * fx) * fy) / 255.0;
    }
}

void Put(DecodedImage &image, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(image.Width) || y >= static_cast<int>(image.Height))
    {
        return;
    }
    auto *p = &image.Rgba[(static_cast<std::size_t>(y) * image.Width + static_cast<std::size_t>(x)) * 4];
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = 255;
}

void Line(DecodedImage &image, double x0, double y0, double x1, double y1)
{
    const int steps = static_cast<int>(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))) + 1;
    for (int i = 0; i <= steps; ++i)
    {
        const double f = static_cast<double>(i) / steps;
        Put(image, static_cast<int>(std::lround(x0 + (x1 - x0) * f)), static_cast<int>(std::lround(y0 + (y1 - y0) * f)), 190, 210, 255);
    }
}
} // namespace

auto RenderRig(const RigScene &scene, const std::vector<RigJointPose> &pose, const RigCamera &camera, std::uint32_t width, std::uint32_t height,
               std::array<std::uint8_t, 3> background, const DecodedImage *texture) -> DecodedImage
{
    return RenderRig(scene, pose, camera, width, height, background, std::vector<const DecodedImage *>(std::max<std::size_t>(scene.Parts.size(), 1), texture));
}

auto RenderRig(const RigScene &scene, const std::vector<RigJointPose> &pose, const RigCamera &camera, std::uint32_t width, std::uint32_t height,
               std::array<std::uint8_t, 3> background, const std::vector<const DecodedImage *> &textures) -> DecodedImage
{
    DecodedImage image;
    image.Width = width;
    image.Height = height;
    image.Rgba.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t i = 0; i < image.Rgba.size(); i += 4)
    {
        image.Rgba[i] = background[0];
        image.Rgba[i + 1] = background[1];
        image.Rgba[i + 2] = background[2];
        image.Rgba[i + 3] = 255;
    }
    if (width < 2 || height < 2 || scene.JointOf.empty() || pose.size() != scene.Parent.size())
    {
        return image;
    }

    // view: turn about +Y (yaw), then about X (pitch); the screen x is the right, y the up, depth towards the camera
    const double cy = std::cos(camera.Yaw), sy = std::sin(camera.Yaw);
    const double cp = std::cos(camera.Pitch), sp = std::sin(camera.Pitch);
    const double scale = std::min(width, height) * 0.45 / scene.Radius * camera.Zoom;
    const double ox = width / 2.0 + camera.PanX;
    const double oy = height / 2.0 + camera.PanY;
    const std::size_t count = scene.JointOf.size();
    // the lightmaps of a level light it in place of the shading by the camera
    const bool lit = camera.Lighting && scene.Lightmap != nullptr && scene.Lightmap->Width > 0 && scene.Lightmap->Height > 0 &&
                     scene.Lightmap->Rgba.size() >= static_cast<std::size_t>(scene.Lightmap->Width) * scene.Lightmap->Height * 4 &&
                     scene.LightUvs.size() == scene.Uvs.size();
    std::vector<Vertex> view(count);
    for (std::size_t v = 0; v < count; ++v)
    {
        const auto &joint = pose[scene.JointOf[v]];
        const double px = scene.Positions[v * 3], py = scene.Positions[v * 3 + 1], pz = scene.Positions[v * 3 + 2];
        const double wx = joint.R[0] * px + joint.R[1] * py + joint.R[2] * pz + joint.O[0] - scene.Center[0];
        const double wy = joint.R[3] * px + joint.R[4] * py + joint.R[5] * pz + joint.O[1] - scene.Center[1];
        const double wz = joint.R[6] * px + joint.R[7] * py + joint.R[8] * pz + joint.O[2] - scene.Center[2];
        const double x1 = cy * wx + sy * wz;
        const double z1 = -sy * wx + cy * wz;
        const double y2 = cp * wy - sp * z1;
        const double z2 = sp * wy + cp * z1;
        view[v] = {ox + x1 * scale, oy - y2 * scale, z2 * scale, scene.Uvs[v * 2], scene.Uvs[v * 2 + 1]};
        if (lit && v * 2 + 1 < scene.LightUvs.size())
        {
            view[v].LU = scene.LightUvs[v * 2] * scene.Lightmap->Width;
            view[v].LV = scene.LightUvs[v * 2 + 1] * scene.Lightmap->Height;
        }
    }

    std::vector<float> depth(static_cast<std::size_t>(width) * height, -std::numeric_limits<float>::infinity());
    // a scene without parts (made by hand) is one part of every triangle
    const std::vector<RigPart> whole{RigPart{0, scene.Indices.size(), scene.ArtSlot, scene.UvSize, scene.Label}};
    const auto &parts = scene.Parts.empty() ? whole : scene.Parts;
    for (std::size_t p = 0; p < parts.size(); ++p)
    {
        const auto &part = parts[p];
        const DecodedImage *texture = p < textures.size() ? textures[p] : nullptr;
        const bool textured = texture != nullptr && texture->Width > 0 && texture->Height > 0 &&
                              texture->Rgba.size() >= static_cast<std::size_t>(texture->Width) * texture->Height * 4;
        const std::size_t end = std::min(scene.Indices.size(), part.FirstIndex + part.IndexCount);
        for (std::size_t t = part.FirstIndex; t + 2 < end; t += 3)
        {
            const Vertex &a = view[scene.Indices[t]];
            const Vertex &b = view[scene.Indices[t + 1]];
            const Vertex &c = view[scene.Indices[t + 2]];
            if (camera.Wireframe)
            {
                Line(image, a.X, a.Y, b.X, b.Y);
                Line(image, b.X, b.Y, c.X, c.Y);
                Line(image, c.X, c.Y, a.X, a.Y);
                continue;
            }
            // flat shading from the geometry of the triangle, lit from the camera on both sides
            const double ux = b.X - a.X, uy = b.Y - a.Y, uz = b.Z - a.Z;
            const double vx = c.X - a.X, vy = c.Y - a.Y, vz = c.Z - a.Z;
            const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
            const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
            const double area2 = ux * vy - uy * vx; // twice the area on screen
            if (length < 1e-12 || std::fabs(area2) < 1e-9)
            {
                continue;
            }
            const double light = 0.38 + 0.62 * std::fabs(nz) / length;
            const int x0 = std::max(0, static_cast<int>(std::floor(std::min({a.X, b.X, c.X}))));
            const int x1 = std::min(static_cast<int>(width) - 1, static_cast<int>(std::ceil(std::max({a.X, b.X, c.X}))));
            const int y0 = std::max(0, static_cast<int>(std::floor(std::min({a.Y, b.Y, c.Y}))));
            const int y1 = std::min(static_cast<int>(height) - 1, static_cast<int>(std::ceil(std::max({a.Y, b.Y, c.Y}))));
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const double px = x + 0.5 - a.X, py = y + 0.5 - a.Y;
                    const double w1 = (px * vy - py * vx) / area2;
                    const double w2 = (ux * py - uy * px) / area2;
                    const double w0 = 1.0 - w1 - w2;
                    if (w0 < -1e-9 || w1 < -1e-9 || w2 < -1e-9)
                    {
                        continue;
                    }
                    const double z = w0 * a.Z + w1 * b.Z + w2 * c.Z;
                    auto &d = depth[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)];
                    if (z <= d)
                    {
                        continue;
                    }
                    d = static_cast<float>(z);
                    double r = 205, g = 205, bl = 215;
                    if (textured)
                    {
                        const double u = (w0 * a.U + w1 * b.U + w2 * c.U) * part.UvSize;
                        const double v = (w0 * a.V + w1 * b.V + w2 * c.V) * part.UvSize;
                        const auto tx = static_cast<std::int64_t>(std::floor(u)) % static_cast<std::int64_t>(texture->Width);
                        const auto ty = static_cast<std::int64_t>(std::floor(v)) % static_cast<std::int64_t>(texture->Height);
                        const auto *p = &texture->Rgba[(static_cast<std::size_t>(ty < 0 ? ty + texture->Height : ty) * texture->Width +
                                                        static_cast<std::size_t>(tx < 0 ? tx + texture->Width : tx)) *
                                                       4];
                        r = p[0];
                        g = p[1];
                        bl = p[2];
                    }
                    double shade[3] = {light, light, light};
                    if (lit)
                    {
                        SampleLight(*scene.Lightmap, w0 * a.LU + w1 * b.LU + w2 * c.LU, w0 * a.LV + w1 * b.LV + w2 * c.LV, shade);
                    }
                    Put(image, x, y, static_cast<std::uint8_t>(std::min(255.0, r * shade[0])), static_cast<std::uint8_t>(std::min(255.0, g * shade[1])),
                        static_cast<std::uint8_t>(std::min(255.0, bl * shade[2])));
                }
            }
        }
    }
    return image;
}
} // namespace Dup::Core
