// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/MeshViewer.h"

#include "Ui/ViewNavigation.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Dup::Ui
{
namespace
{
constexpr std::size_t MAX_SHADED_TRIANGLES = 40000;
}

MeshViewer::MeshViewer(QWidget *parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(120, 120);
    setContextMenuPolicy(Qt::PreventContextMenu); // the right button moves the view
    setObjectName("viewCanvas"); // found by the test hook MainWindow::TestViewDrag
}

void MeshViewer::SetMesh(std::shared_ptr<const Core::Mesh> mesh)
{
    _mesh = std::move(mesh);
    _zoom = 1.0;
    _yaw = 0.6;
    _pitch = 0.4;
    _panX = 0.0;
    _panY = 0.0;
    update();
}

auto MeshViewer::GetDescription() const -> QString
{
    return _mesh == nullptr ? QString() : tr("Mesh: %1 vertices, %2 triangles").arg(_mesh->Positions.size() / 3).arg(_mesh->Triangles.size() / 3);
}

void MeshViewer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Base));
    if (_mesh == nullptr)
    {
        return;
    }
    ViewNavigation::DrawHint(painter, rect(), palette().color(QPalette::Text)); // under the model: it stays in its corner
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto &mesh = *_mesh;
    const std::size_t vertexCount = mesh.Positions.size() / 3;
    const double centre[3] = {(mesh.Min[0] + mesh.Max[0]) / 2.0, (mesh.Min[1] + mesh.Max[1]) / 2.0, (mesh.Min[2] + mesh.Max[2]) / 2.0};
    double radius = 0.0;
    for (int axis = 0; axis < 3; ++axis)
    {
        radius = std::max(radius, (mesh.Max[axis] - mesh.Min[axis]) / 2.0);
    }
    if (radius <= 0.0)
    {
        radius = 1.0;
    }
    const double scale = std::min(width(), height()) * 0.45 / radius * _zoom;
    const double cy = std::cos(_yaw);
    const double sy = std::sin(_yaw);
    const double cp = std::cos(_pitch);
    const double sp = std::sin(_pitch);

    // rotate around the centre: yaw around the vertical axis, then pitch; z is the depth (towards the viewer)
    struct Projected
    {
        double X, Y, Z;
    };
    std::vector<Projected> projected(vertexCount);
    for (std::size_t v = 0; v < vertexCount; ++v)
    {
        const double x = mesh.Positions[v * 3] - centre[0];
        const double y = mesh.Positions[v * 3 + 1] - centre[1];
        const double z = mesh.Positions[v * 3 + 2] - centre[2];
        const double x1 = x * cy + z * sy;
        const double z1 = -x * sy + z * cy;
        const double y2 = y * cp - z1 * sp;
        const double z2 = y * sp + z1 * cp;
        projected[v] = {width() / 2.0 + _panX + x1 * scale, height() / 2.0 + _panY - y2 * scale, z2 * scale}; // the same scale on all axes: the normals stay right
    }

    const std::size_t triangleCount = mesh.Triangles.size() / 3;
    const QColor line = palette().color(QPalette::Highlight);
    if (_wireframe || triangleCount > MAX_SHADED_TRIANGLES)
    {
        painter.setPen(QPen(line, 1));
        const std::size_t step = triangleCount > 200000 ? 2 : 1; // a very big mesh: half of the edges are enough
        for (std::size_t t = 0; t < triangleCount; t += step)
        {
            const auto &a = projected[mesh.Triangles[t * 3]];
            const auto &b = projected[mesh.Triangles[t * 3 + 1]];
            const auto &c = projected[mesh.Triangles[t * 3 + 2]];
            const QPointF points[4] = {{a.X, a.Y}, {b.X, b.Y}, {c.X, c.Y}, {a.X, a.Y}};
            painter.drawPolyline(points, 4);
        }
        return;
    }

    std::vector<std::uint32_t> order(triangleCount);
    std::iota(order.begin(), order.end(), 0U);
    std::vector<double> depth(triangleCount);
    for (std::size_t t = 0; t < triangleCount; ++t)
    {
        depth[t] = projected[mesh.Triangles[t * 3]].Z + projected[mesh.Triangles[t * 3 + 1]].Z + projected[mesh.Triangles[t * 3 + 2]].Z;
    }
    std::sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) { return depth[a] < depth[b]; });
    painter.setPen(Qt::NoPen);
    const QColor base = palette().color(QPalette::Highlight).lighter(130);
    for (const auto t : order)
    {
        const auto &a = projected[mesh.Triangles[t * 3]];
        const auto &b = projected[mesh.Triangles[t * 3 + 1]];
        const auto &c = projected[mesh.Triangles[t * 3 + 2]];
        // the normal of the projected triangle gives the facing and the light (from the viewer, a little from above)
        const double ux = b.X - a.X, uy = b.Y - a.Y, uz = b.Z - a.Z;
        const double vx = c.X - a.X, vy = c.Y - a.Y, vz = c.Z - a.Z;
        double nx = uy * vz - uz * vy;
        double ny = uz * vx - ux * vz;
        double nz = ux * vy - uy * vx;
        const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (length <= 1e-9)
        {
            continue;
        }
        nx /= length;
        ny /= length;
        nz /= length;
        const double light = std::abs(0.25 * nx - 0.35 * ny + 0.9 * nz); // two sided: the winding of game meshes varies
        painter.setBrush(QColor::fromRgbF(std::min(1.0, base.redF() * (0.25 + 0.75 * light)), std::min(1.0, base.greenF() * (0.25 + 0.75 * light)),
                                          std::min(1.0, base.blueF() * (0.25 + 0.75 * light))));
        const QPointF points[3] = {{a.X, a.Y}, {b.X, b.Y}, {c.X, c.Y}};
        painter.drawPolygon(points, 3);
    }
}

void MeshViewer::mousePressEvent(QMouseEvent *event)
{
    ViewNavigation::Press(event, _last);
    setFocus();
}

void MeshViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (ViewNavigation::Move(event, _last, {_yaw, _pitch, _zoom, _panX, _panY, 1.0}))
    {
        update();
    }
}

void MeshViewer::wheelEvent(QWheelEvent *event)
{
    if (ViewNavigation::Wheel(event, size(), {_yaw, _pitch, _zoom, _panX, _panY, 1.0}))
    {
        update();
    }
}

void MeshViewer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_W)
    {
        _wireframe = !_wireframe;
    }
    else if (event->key() == Qt::Key_F)
    {
        _zoom = 1.0;
        _yaw = 0.6;
        _pitch = 0.4;
        _panX = 0.0;
        _panY = 0.0;
    }
    else
    {
        const auto all = ViewNavigation::AllViewerKeys(); // W, F, B, Space: W and F here
        if (!ViewNavigation::Key(event, this, {_yaw, _pitch, _zoom, _panX, _panY, 1.0}, {all[0], all[1]}))
        {
            QWidget::keyPressEvent(event);
            return;
        }
    }
    update();
}
} // namespace Dup::Ui
