// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/OofRender.h"

#include <QWidget>

#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;

namespace Dup::Ui
{
class ModelCanvas;

/// The viewer of a Descent 3 model: textured surfaces, billboards and glows drawn by the software renderer of Core, with the
/// animation of the model on a slider (and a play button that also turns the $rotate subobjects). Drag to turn the model, wheel to
/// zoom, W for wireframe, B to see the back faces too, F to frame it again.
class ModelViewer : public QWidget
{
    Q_OBJECT

  public:
    explicit ModelViewer(QWidget *parent = nullptr);
    ~ModelViewer() override;

    /// nullptr clears the viewer (and stops the animation).
    void SetScene(std::shared_ptr<const Core::OofScene> scene);
    [[nodiscard]] auto GetDescription() const -> QString;
    /// Test hooks.
    void SetAnimation(bool animated, double u);
    [[nodiscard]] auto GetPoseNotes() const -> QStringList;

  protected:
    void hideEvent(QHideEvent *event) override;

  private:
    friend class ModelCanvas;
    void UpdateControls();
    void OnTick();
    void StopPlaying();
    [[nodiscard]] auto CurrentPose() const -> Core::OofPose;

    std::shared_ptr<const Core::OofScene> _scene;
    ModelCanvas *_canvas;
    QWidget *_controls;
    QCheckBox *_animate;
    QPushButton *_play;
    QSlider *_slider;
    QComboBox *_speed;
    QLabel *_hint;
    QTimer *_timer;
    Core::OofCamera _camera;
    bool _rest{true};
    double _u{0};
    double _seconds{0};
    double _speedFactor{1.0}; // times the normal speed of the playback
    bool _playing{false};
};
} // namespace Dup::Ui
