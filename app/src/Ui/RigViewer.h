// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/RigRender.h"

#include <QWidget>

#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;

namespace Dup::Ui
{
class RigCanvas;

/// The viewer of a skinned model with animations (the glTF of the Requiem models): the textured model drawn by the software
/// renderer of Core, a list of its animations, a play button, a slider over the animation and a speed. Drag to turn the model,
/// wheel to zoom, W for wireframe, F to frame it again.
class RigViewer : public QWidget
{
    Q_OBJECT

  public:
    explicit RigViewer(QWidget *parent = nullptr);
    ~RigViewer() override;

    /// nullptr clears the viewer (and stops the animation).
    /// textures: the picture of each part of the scene (nullptr: none)
    void SetScene(std::shared_ptr<const Core::RigScene> scene, std::vector<std::shared_ptr<const Core::DecodedImage>> textures);
    [[nodiscard]] auto GetDescription() const -> QString;
    /// Test hooks.
    void SelectAnimation(int index);
    void SetPosition(double u);
    [[nodiscard]] auto CurrentFrameImage(std::uint32_t width, std::uint32_t height) const -> Core::DecodedImage;

  protected:
    void hideEvent(QHideEvent *event) override;

  private:
    friend class RigCanvas;
    void UpdateControls();
    void OnTick();
    void StopPlaying();
    [[nodiscard]] auto Seconds() const -> double;

    std::shared_ptr<const Core::RigScene> _scene;
    std::vector<std::shared_ptr<const Core::DecodedImage>> _textures;
    RigCanvas *_canvas;
    QWidget *_controls;
    QComboBox *_animations;
    QPushButton *_play;
    QSlider *_slider;
    QComboBox *_speed;
    QLabel *_hint;
    QTimer *_timer;
    Core::RigCamera _camera;
    int _animation{0}; // -1: the rest pose of the nodes
    double _u{0};      // position in the animation, 0 to 1
    double _speedFactor{1.0};
    bool _playing{false};
};
} // namespace Dup::Ui
