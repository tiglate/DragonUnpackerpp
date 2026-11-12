// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ObjParser.h"

#include <QWidget>

#include <memory>

namespace Dup::Ui
{
/// A small software 3D view of a mesh: drag with the left button to turn it, with the right one to move it, wheel to zoom,
/// W to switch between the shaded and the wireframe drawing, F to frame the model again, H for the help. Triangles are sorted back to front (painter's algorithm) up to
/// 40,000 of them, bigger meshes are drawn as wireframe.
class MeshViewer : public QWidget
{
    Q_OBJECT

  public:
    explicit MeshViewer(QWidget *parent = nullptr);

    void SetMesh(std::shared_ptr<const Core::Mesh> mesh);
    [[nodiscard]] auto GetDescription() const -> QString;

  protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

  private:
    std::shared_ptr<const Core::Mesh> _mesh;
    double _yaw{0.6};
    double _pitch{0.4};
    double _zoom{1.0};
    double _panX{0.0};
    double _panY{0.0};
    bool _wireframe{false};
    QPoint _last;
};
} // namespace Dup::Ui
