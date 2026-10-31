// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A small software renderer for Descent 3 models (see the "Assembling and displaying an OOF" section of the specification).

#include "Core/OofRender.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Dup::Core
{
namespace
{
using Vec = std::array<double, 3>;

auto IsFinite(const Vec &v) -> bool
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

auto ToVec(const OofVec3 &v) -> Vec
{
    return {static_cast<double>(v[0]), static_cast<double>(v[1]), static_cast<double>(v[2])};
}

auto Cross(const Vec &a, const Vec &b) -> Vec
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

auto Sub(const Vec &a, const Vec &b) -> Vec
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

auto Length(const Vec &v) -> double
{
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/// The view: the file's coordinates (left-handed, +Z forward) become right-handed by negating Z, then yaw about the vertical axis
/// and pitch; the result has X to the right, Y up and Z towards the viewer.
struct View
{
    double Cy, Sy, Cp, Sp, Scale, CentreX, CentreY;
    Vec Center;

    /// A vector already in right-handed coordinates, turned by the camera.
    [[nodiscard]] auto Turn(const Vec &v) const -> Vec
    {
        const double x1 = v[0] * Cy + v[2] * Sy;
        const double z1 = -v[0] * Sy + v[2] * Cy;
        return {x1, v[1] * Cp - z1 * Sp, v[1] * Sp + z1 * Cp};
    }
    /// A direction of the file (left-handed).
    [[nodiscard]] auto Direction(const Vec &d) const -> Vec
    {
        return Turn({d[0], d[1], -d[2]});
    }
    /// Screen X, screen Y and depth (larger is nearer) of a point of the model, in the coordinates of the file.
    [[nodiscard]] auto Project(const Vec &p) const -> Vec
    {
        const auto d = Turn({p[0] - Center[0], p[1] - Center[1], -p[2] - Center[2]});
        return {CentreX + d[0] * Scale, CentreY - d[1] * Scale, d[2] * Scale};
    }
};

struct Canvas
{
    std::uint32_t Width, Height;
    std::vector<std::uint8_t> Rgba;
    std::vector<float> Depth;

    void Blend(std::int64_t x, std::int64_t y, double r, double g, double b, double alpha)
    {
        auto *px = &Rgba[(static_cast<std::size_t>(y) * Width + static_cast<std::size_t>(x)) * 4];
        px[0] = static_cast<std::uint8_t>(std::clamp(px[0] + (r - px[0]) * alpha, 0.0, 255.0));
        px[1] = static_cast<std::uint8_t>(std::clamp(px[1] + (g - px[1]) * alpha, 0.0, 255.0));
        px[2] = static_cast<std::uint8_t>(std::clamp(px[2] + (b - px[2]) * alpha, 0.0, 255.0));
    }
    void Add(std::int64_t x, std::int64_t y, double r, double g, double b, double alpha)
    {
        auto *px = &Rgba[(static_cast<std::size_t>(y) * Width + static_cast<std::size_t>(x)) * 4];
        px[0] = static_cast<std::uint8_t>(std::clamp(px[0] + r * alpha, 0.0, 255.0));
        px[1] = static_cast<std::uint8_t>(std::clamp(px[1] + g * alpha, 0.0, 255.0));
        px[2] = static_cast<std::uint8_t>(std::clamp(px[2] + b * alpha, 0.0, 255.0));
    }
};

/// One corner after the transform: screen position, depth, texture coordinates, light and opacity.
struct Corner
{
    double X, Y, Z, U, V, Shade, Alpha;
};

struct Surface
{
    std::vector<Corner> Corners; // the polygon, fan triangulated when drawn
    const OofMaterial *Material{nullptr};
    std::array<std::uint8_t, 3> Color{};
    bool Textured{false};
    double Depth{0}; // mean depth, to sort the translucent ones
};

auto Texel(const DecodedImage &image, double u, double v) -> const std::uint8_t *
{
    if (!std::isfinite(u) || !std::isfinite(v))
    {
        u = v = 0;
    }
    // (0,0) is the top left corner, V grows downwards; the addressing wraps
    const double fu = u - std::floor(u), fv = v - std::floor(v);
    const auto x = std::min<std::uint32_t>(image.Width - 1, static_cast<std::uint32_t>(fu * image.Width));
    const auto y = std::min<std::uint32_t>(image.Height - 1, static_cast<std::uint32_t>(fv * image.Height));
    return &image.Rgba[(static_cast<std::size_t>(y) * image.Width + x) * 4];
}

void Shade(const Surface &surface, const Corner &c, double &r, double &g, double &b, double &a)
{
    if (surface.Textured)
    {
        if (surface.Material != nullptr && surface.Material->Image != nullptr)
        {
            const auto *t = Texel(*surface.Material->Image, c.U, c.V);
            r = t[0];
            g = t[1];
            b = t[2];
            a = t[3] / 255.0;
        }
        else
        {
            // a face whose texture was not found: a checkerboard that keeps the UVs visible
            const bool dark = (static_cast<std::int64_t>(std::floor(c.U * 8.0)) + static_cast<std::int64_t>(std::floor(c.V * 8.0))) % 2 != 0;
            r = dark ? 60 : 255;
            g = dark ? 0 : 0;
            b = dark ? 60 : 255;
            a = 1.0;
        }
    }
    else
    {
        r = surface.Color[0];
        g = surface.Color[1];
        b = surface.Color[2];
        a = 1.0;
    }
    r *= c.Shade;
    g *= c.Shade;
    b *= c.Shade;
    a *= c.Alpha;
}

/// Fills one triangle. With write the depth buffer is updated (opaque surfaces: texels with no alpha are skipped); without it
/// the colour is blended by the alpha (translucent surfaces).
void DrawTriangle(Canvas &canvas, const Surface &surface, const Corner &a, const Corner &b, const Corner &c, bool opaquePass)
{
    const double area = (b.X - a.X) * (c.Y - a.Y) - (b.Y - a.Y) * (c.X - a.X);
    if (!(std::abs(area) > 1e-9))
    {
        return;
    }
    const auto minX = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor(std::min({a.X, b.X, c.X}))));
    const auto maxX = std::min<std::int64_t>(canvas.Width - 1, static_cast<std::int64_t>(std::ceil(std::max({a.X, b.X, c.X}))));
    const auto minY = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::floor(std::min({a.Y, b.Y, c.Y}))));
    const auto maxY = std::min<std::int64_t>(canvas.Height - 1, static_cast<std::int64_t>(std::ceil(std::max({a.Y, b.Y, c.Y}))));
    for (auto y = minY; y <= maxY; ++y)
    {
        for (auto x = minX; x <= maxX; ++x)
        {
            const double px = static_cast<double>(x) + 0.5, py = static_cast<double>(y) + 0.5;
            const double w0 = ((b.X - px) * (c.Y - py) - (b.Y - py) * (c.X - px)) / area;
            const double w1 = ((c.X - px) * (a.Y - py) - (c.Y - py) * (a.X - px)) / area;
            const double w2 = 1.0 - w0 - w1;
            constexpr double EDGE = -1e-9;
            if (w0 < EDGE || w1 < EDGE || w2 < EDGE)
            {
                continue;
            }
            const double z = w0 * a.Z + w1 * b.Z + w2 * c.Z;
            const auto index = static_cast<std::size_t>(y) * canvas.Width + static_cast<std::size_t>(x);
            if (z <= canvas.Depth[index])
            {
                continue;
            }
            Corner at{px, py, z, w0 * a.U + w1 * b.U + w2 * c.U, w0 * a.V + w1 * b.V + w2 * c.V, w0 * a.Shade + w1 * b.Shade + w2 * c.Shade,
                      w0 * a.Alpha + w1 * b.Alpha + w2 * c.Alpha};
            double r, g, bl, alpha;
            Shade(surface, at, r, g, bl, alpha);
            if (opaquePass)
            {
                if (alpha < 0.5)
                {
                    continue; // a transparent texel: nothing is drawn, the depth stays
                }
                canvas.Depth[index] = static_cast<float>(z);
                canvas.Blend(x, y, r, g, bl, 1.0);
            }
            else if (alpha > 0.0)
            {
                canvas.Blend(x, y, r, g, bl, alpha);
            }
        }
    }
}

void DrawLine(Canvas &canvas, double x0, double y0, double x1, double y1, std::array<std::uint8_t, 3> colour)
{
    const double length = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    if (!(length < 20000.0))
    {
        return;
    }
    const int steps = std::max(1, static_cast<int>(std::ceil(length)));
    for (int i = 0; i <= steps; ++i)
    {
        const double t = static_cast<double>(i) / steps;
        const auto x = static_cast<std::int64_t>(std::lround(x0 + (x1 - x0) * t));
        const auto y = static_cast<std::int64_t>(std::lround(y0 + (y1 - y0) * t));
        if (x >= 0 && y >= 0 && x < canvas.Width && y < canvas.Height)
        {
            canvas.Blend(x, y, colour[0], colour[1], colour[2], 1.0);
        }
    }
}

/// The area of a polygon (corner positions in the model's own coordinates).
auto PolygonArea(const OofSubobject &so, const OofFace &face) -> double
{
    Vec sum{0, 0, 0};
    const Vec origin = ToVec(so.Positions[static_cast<std::size_t>(face.Corners[0].Vertex)]);
    for (std::size_t k = 2; k < face.Corners.size(); ++k)
    {
        const auto n = Cross(Sub(ToVec(so.Positions[static_cast<std::size_t>(face.Corners[k - 1].Vertex)]), origin),
                             Sub(ToVec(so.Positions[static_cast<std::size_t>(face.Corners[k].Vertex)]), origin));
        for (int i = 0; i < 3; ++i)
        {
            sum[static_cast<std::size_t>(i)] += n[static_cast<std::size_t>(i)];
        }
    }
    return Length(sum) / 2.0;
}

/// Glow colours are written 0 to 1; values above that are taken as 0 to 255.
auto GlowColour(double v, bool bytes) -> double
{
    return std::clamp(bytes ? v : v * 255.0, 0.0, 255.0);
}
} // namespace

auto MakeOofScene(std::shared_ptr<const OofModel> model, const OofTextureResolver &resolver) -> std::shared_ptr<const OofScene>
{
    auto scene = std::make_shared<OofScene>();
    scene->Model = model;
    for (const auto &name : model->Textures)
    {
        OofMaterial material;
        material.Name = name;
        if (resolver)
        {
            material.Image = resolver(name);
        }
        if (material.Image != nullptr)
        {
            ++scene->ResolvedTextures;
            for (std::size_t i = 3; i < material.Image->Rgba.size(); i += 4)
            {
                if (material.Image->Rgba[i] != 0 && material.Image->Rgba[i] != 255)
                {
                    material.Translucent = true;
                    break;
                }
            }
        }
        else
        {
            ++scene->MissingTextures;
        }
        scene->Materials.push_back(std::move(material));
    }
    for (const auto &so : model->Subobjects)
    {
        scene->Properties.push_back(ParseOofProperties(so.Properties));
    }
    scene->Polygons = OofDrawnSubobjects(*model);
    scene->Animated = HasOofAnimation(*model);
    // the bounds of the unanimated pose, in the coordinates of the view (Z negated)
    const auto rest = EvaluateOofPose(*model, {});
    Vec lo{0, 0, 0}, hi{0, 0, 0};
    bool first = true;
    auto include = [&](const Vec &p) {
        const Vec q{p[0], p[1], -p[2]};
        for (std::size_t c = 0; c < 3; ++c)
        {
            lo[c] = first ? q[c] : std::min(lo[c], q[c]);
            hi[c] = first ? q[c] : std::max(hi[c], q[c]);
        }
        first = false;
    };
    for (std::size_t s = 0; s < model->Subobjects.size(); ++s)
    {
        if (!scene->Polygons[s])
        {
            continue;
        }
        for (const auto &p : model->Subobjects[s].Positions)
        {
            include(rest.World[s].Apply(ToVec(p)));
        }
        for (const auto &face : model->Subobjects[s].Faces)
        {
            scene->Triangles += face.Corners.size() >= 3 ? face.Corners.size() - 2 : 0;
        }
    }
    if (first)
    {
        for (const auto &t : rest.World)
        {
            include(t.T);
        }
    }
    double radius = 0;
    for (std::size_t c = 0; c < 3; ++c)
    {
        scene->Center[c] = (lo[c] + hi[c]) / 2.0;
        radius = std::max(radius, (hi[c] - lo[c]) / 2.0);
    }
    scene->Radius = radius > 0 ? radius : 1.0;
    return scene;
}

auto RenderOof(const OofScene &scene, const OofPose &pose, const OofCamera &camera, std::uint32_t width, std::uint32_t height,
               std::array<std::uint8_t, 3> background) -> DecodedImage
{
    Canvas canvas{width, height, {}, {}};
    canvas.Rgba.resize(static_cast<std::size_t>(width) * height * 4);
    canvas.Depth.assign(static_cast<std::size_t>(width) * height, -std::numeric_limits<float>::infinity());
    for (std::size_t i = 0; i < canvas.Rgba.size(); i += 4)
    {
        canvas.Rgba[i] = background[0];
        canvas.Rgba[i + 1] = background[1];
        canvas.Rgba[i + 2] = background[2];
        canvas.Rgba[i + 3] = 255;
    }
    const auto &model = *scene.Model;
    const View view{std::cos(camera.Yaw), std::sin(camera.Yaw), std::cos(camera.Pitch), std::sin(camera.Pitch),
                    std::min(width, height) * 0.45 / scene.Radius * camera.Zoom, width / 2.0, height / 2.0, scene.Center};

    // light: the direction the light travels in the view (from the viewer, a little from above and the left)
    const Vec light = []() {
        const Vec l{0.35, -0.5, -0.8};
        const double n = Length(l);
        return Vec{l[0] / n, l[1] / n, l[2] / n};
    }();
    auto brightness = [&](const Vec &normalView) {
        const double dot = normalView[0] * light[0] + normalView[1] * light[1] + normalView[2] * light[2];
        return std::clamp(0.3 + 0.7 * (1.0 - dot) / 2.0, 0.0, 1.0);
    };

    std::vector<Surface> opaque, translucent;
    for (std::size_t s = 0; s < model.Subobjects.size(); ++s)
    {
        if (!scene.Polygons[s])
        {
            continue;
        }
        const auto &so = model.Subobjects[s];
        const auto &world = pose.World[s];
        std::vector<Vec> screen(so.Positions.size()), normals(so.Positions.size());
        std::vector<Vec> modelPoints(so.Positions.size());
        for (std::size_t v = 0; v < so.Positions.size(); ++v)
        {
            modelPoints[v] = world.Apply(ToVec(so.Positions[v]));
            screen[v] = view.Project(modelPoints[v]);
            normals[v] = view.Direction(world.Rotate(ToVec(so.Normals[v])));
        }
        for (const auto &face : so.Faces)
        {
            if (face.Corners.size() < 3)
            {
                continue;
            }
            // the visibility test of the game: the face is seen when its stored normal points to the eye
            Vec faceNormal = view.Direction(world.Rotate(ToVec(face.Normal)));
            if (!IsFinite(faceNormal) || Length(faceNormal) < 1e-9)
            {
                const auto &p0 = modelPoints[static_cast<std::size_t>(face.Corners[0].Vertex)];
                const auto &p1 = modelPoints[static_cast<std::size_t>(face.Corners[1].Vertex)];
                const auto &p2 = modelPoints[static_cast<std::size_t>(face.Corners[2].Vertex)];
                // a degenerate stored normal: use the geometry, as seen in the view (the file's winding is clockwise)
                faceNormal = view.Direction(Cross(Sub(p2, p0), Sub(p1, p0)));
            }
            if (camera.CullBackFaces && faceNormal[2] < 0)
            {
                continue;
            }
            Surface surface;
            surface.Textured = face.Texture >= 0;
            if (surface.Textured)
            {
                surface.Material = &scene.Materials[static_cast<std::size_t>(face.Texture)];
            }
            surface.Color = face.Color;
            bool vertexAlpha = false;
            for (const auto &corner : face.Corners)
            {
                const auto v = static_cast<std::size_t>(corner.Vertex);
                const double alpha = v < so.Alpha.size() && std::isfinite(so.Alpha[v]) ? std::clamp<double>(so.Alpha[v], 0.0, 1.0) : 1.0;
                vertexAlpha = vertexAlpha || alpha < 0.999;
                const double b = IsFinite(normals[v]) ? brightness(normals[v]) : 0.8;
                surface.Corners.push_back({screen[v][0], screen[v][1], screen[v][2], static_cast<double>(corner.U), static_cast<double>(corner.V), b, alpha});
                surface.Depth += screen[v][2];
            }
            surface.Depth /= static_cast<double>(face.Corners.size());
            ((surface.Material != nullptr && surface.Material->Translucent) || vertexAlpha ? translucent : opaque).push_back(std::move(surface));
        }
    }

    auto draw = [&](const Surface &surface, bool opaquePass) {
        if (camera.Wireframe)
        {
            for (std::size_t k = 0; k < surface.Corners.size(); ++k)
            {
                const auto &a = surface.Corners[k];
                const auto &b = surface.Corners[(k + 1) % surface.Corners.size()];
                DrawLine(canvas, a.X, a.Y, b.X, b.Y, {80, 140, 220});
            }
            return;
        }
        for (std::size_t k = 2; k < surface.Corners.size(); ++k)
        {
            DrawTriangle(canvas, surface, surface.Corners[0], surface.Corners[k - 1], surface.Corners[k], opaquePass);
        }
    };
    for (const auto &surface : opaque)
    {
        draw(surface, true);
    }
    std::stable_sort(translucent.begin(), translucent.end(), [](const Surface &a, const Surface &b) { return a.Depth < b.Depth; });
    for (const auto &surface : translucent)
    {
        draw(surface, false);
    }

    // the second pass: billboards and glows at the transformed origin of their subobject, without writing depth
    for (std::size_t s = 0; s < model.Subobjects.size() && !camera.Wireframe; ++s)
    {
        const auto &properties = scene.Properties[s];
        const auto &so = model.Subobjects[s];
        const bool glow = properties.Glow || properties.Thruster || (so.Properties.find("$glow") != std::string::npos);
        if (!properties.Facing && !glow)
        {
            continue;
        }
        const auto origin = view.Project(pose.World[s].T);
        if (!IsFinite(origin))
        {
            continue;
        }
        if (properties.Facing && !so.Faces.empty() && so.Faces[0].Corners.size() >= 3 && !glow)
        {
            const auto &face = so.Faces[0];
            const double area = PolygonArea(so, face);
            double halfW = std::sqrt(area) / 2.0, halfH = halfW;
            const OofMaterial *material = face.Texture >= 0 ? &scene.Materials[static_cast<std::size_t>(face.Texture)] : nullptr;
            if (material != nullptr && material->Image != nullptr && material->Image->Width > 0)
            {
                halfH = halfW * material->Image->Height / material->Image->Width;
            }
            // the game's texture database says whether a billboard adds its light (SATURATE); without it, a picture that has no
            // transparency at all is taken as a light sprite (a flare), one with a mask is blended normally
            bool additive = material != nullptr && material->Image != nullptr;
            if (additive)
            {
                for (std::size_t i = 3; i < material->Image->Rgba.size(); i += 4)
                {
                    if (material->Image->Rgba[i] != 255)
                    {
                        additive = false;
                        break;
                    }
                }
            }
            const double sw = halfW * view.Scale, sh = halfH * view.Scale;
            for (auto y = std::max<std::int64_t>(0, static_cast<std::int64_t>(origin[1] - sh)); y <= std::min<std::int64_t>(height - 1, static_cast<std::int64_t>(origin[1] + sh)); ++y)
            {
                for (auto x = std::max<std::int64_t>(0, static_cast<std::int64_t>(origin[0] - sw)); x <= std::min<std::int64_t>(width - 1, static_cast<std::int64_t>(origin[0] + sw)); ++x)
                {
                    if (origin[2] <= canvas.Depth[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)])
                    {
                        continue;
                    }
                    const double u = sw > 0 ? (x + 0.5 - (origin[0] - sw)) / (2 * sw) : 0.5;
                    const double v = sh > 0 ? (y + 0.5 - (origin[1] - sh)) / (2 * sh) : 0.5;
                    if (material != nullptr && material->Image != nullptr)
                    {
                        const auto *t = Texel(*material->Image, u, v);
                        if (additive)
                        {
                            canvas.Add(x, y, t[0], t[1], t[2], 1.0);
                        }
                        else if (t[3] != 0)
                        {
                            canvas.Blend(x, y, t[0], t[1], t[2], t[3] / 255.0);
                        }
                    }
                    else
                    {
                        canvas.Blend(x, y, 255, 0, 255, 0.5);
                    }
                }
            }
            continue;
        }
        if (glow)
        {
            const auto &values = properties.Glow ? properties.Glow : properties.Thruster;
            std::array<double, 4> v = values ? *values : std::array<double, 4>{1, 1, 1, 1};
            const bool bytes = std::max({v[0], v[1], v[2]}) > 1.0;
            const double radius = std::max(1.0, v[3] * view.Scale);
            const double r = GlowColour(v[0], bytes), g = GlowColour(v[1], bytes), b = GlowColour(v[2], bytes);
            // the game gives glows a depth bias equal to their size so that they do not sink into their emitter
            const double depth = origin[2] + v[3] * view.Scale;
            for (auto y = std::max<std::int64_t>(0, static_cast<std::int64_t>(origin[1] - radius)); y <= std::min<std::int64_t>(height - 1, static_cast<std::int64_t>(origin[1] + radius)); ++y)
            {
                for (auto x = std::max<std::int64_t>(0, static_cast<std::int64_t>(origin[0] - radius)); x <= std::min<std::int64_t>(width - 1, static_cast<std::int64_t>(origin[0] + radius)); ++x)
                {
                    const double dx = (x + 0.5 - origin[0]) / radius, dy = (y + 0.5 - origin[1]) / radius;
                    const double d2 = dx * dx + dy * dy;
                    if (d2 >= 1.0 || depth <= canvas.Depth[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)])
                    {
                        continue;
                    }
                    const double falloff = (1.0 - d2) * (1.0 - d2); // a generated gradient: the game's image comes from its effect assets
                    canvas.Add(x, y, r, g, b, 0.8 * falloff);
                }
            }
        }
    }
    DecodedImage image;
    image.Width = width;
    image.Height = height;
    image.Rgba = std::move(canvas.Rgba);
    return image;
}
} // namespace Dup::Core
