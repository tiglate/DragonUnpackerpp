// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/RigViewer.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace Dup::Ui
{
namespace
{
constexpr int SLIDER_STEPS = 1000;
} // namespace

/// The drawing surface: renders the scene in the pose of the moment on every paint.
class RigCanvas : public QWidget
{
  public:
    explicit RigCanvas(RigViewer *owner) : QWidget(owner), _owner(owner)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(120, 120);
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        const QColor background(30, 32, 38);
        painter.fillRect(rect(), background);
        if (_owner->_scene == nullptr || width() < 2 || height() < 2)
        {
            return;
        }
        const auto image = _owner->CurrentFrameImage(static_cast<std::uint32_t>(width()), static_cast<std::uint32_t>(height()));
        const QImage qimage(image.Rgba.data(), static_cast<int>(image.Width), static_cast<int>(image.Height), static_cast<qsizetype>(image.Width) * 4, QImage::Format_RGBA8888);
        painter.drawImage(0, 0, qimage);
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        _last = event->pos();
        setFocus();
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if ((event->buttons() & Qt::LeftButton) != 0)
        {
            auto &camera = _owner->_camera;
            camera.Yaw -= (event->pos().x() - _last.x()) * 0.01;
            camera.Pitch = std::clamp(camera.Pitch + (event->pos().y() - _last.y()) * 0.01, -1.5, 1.5);
            _last = event->pos();
            update();
        }
    }
    void wheelEvent(QWheelEvent *event) override
    {
        auto &camera = _owner->_camera;
        camera.Zoom = std::clamp(camera.Zoom * (event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15), 0.1, 50.0);
        update();
        event->accept();
    }
    void keyPressEvent(QKeyEvent *event) override
    {
        auto &camera = _owner->_camera;
        if (event->key() == Qt::Key_W)
        {
            camera.Wireframe = !camera.Wireframe;
        }
        else if (event->key() == Qt::Key_F)
        {
            camera = Core::RigCamera{};
        }
        else if (event->key() == Qt::Key_Space)
        {
            _owner->_play->click();
        }
        else
        {
            QWidget::keyPressEvent(event);
            return;
        }
        update();
    }

  private:
    RigViewer *_owner;
    QPoint _last;
};

RigViewer::RigViewer(QWidget *parent)
    : QWidget(parent), _canvas(new RigCanvas(this)), _controls(new QWidget(this)), _animations(new QComboBox(_controls)), _play(new QPushButton(tr("Play"), _controls)),
      _slider(new QSlider(Qt::Horizontal, _controls)), _speed(new QComboBox(_controls)), _hint(new QLabel(_controls)), _timer(new QTimer(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_canvas, 1);
    layout->addWidget(_controls);
    auto *row = new QHBoxLayout(_controls);
    row->setContentsMargins(6, 2, 6, 2);
    row->addWidget(_animations);
    row->addWidget(_play);
    row->addWidget(_slider, 1);
    row->addWidget(_speed);
    row->addWidget(_hint);
    _slider->setRange(0, SLIDER_STEPS);
    _animations->setToolTip(tr("The animation to show"));
    _animations->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _animations->setMinimumContentsLength(12);
    for (const double factor : {0.1, 0.25, 0.5, 1.0, 2.0})
    {
        _speed->addItem(QString("%1x").arg(factor), factor);
    }
    _speed->setCurrentIndex(3);
    _speed->setToolTip(tr("Speed of the playback (1x is the speed of the game, 25 frames per second)"));
    _timer->setInterval(33);
    _controls->hide();

    connect(_animations, &QComboBox::currentIndexChanged, this, [this](int row) {
        _animation = row < 0 ? -1 : _animations->itemData(row).toInt();
        _u = 0;
        const QSignalBlocker blocker(_slider);
        _slider->setValue(0);
        UpdateControls();
        _canvas->update();
    });
    connect(_slider, &QSlider::sliderPressed, this, [this] { StopPlaying(); });
    connect(_slider, &QSlider::valueChanged, this, [this](int value) {
        _u = static_cast<double>(value) / SLIDER_STEPS;
        UpdateControls();
        _canvas->update();
    });
    connect(_play, &QPushButton::clicked, this, [this] {
        if (_playing)
        {
            StopPlaying();
            return;
        }
        if (_scene == nullptr || _animation < 0)
        {
            return;
        }
        _playing = true;
        _play->setText(tr("Pause"));
        _timer->start();
    });
    connect(_speed, &QComboBox::currentIndexChanged, this, [this](int row) { _speedFactor = _speed->itemData(row).toDouble(); });
    connect(_timer, &QTimer::timeout, this, [this] { OnTick(); });
}

RigViewer::~RigViewer() = default;

void RigViewer::SetScene(std::shared_ptr<const Core::RigScene> scene, std::vector<std::shared_ptr<const Core::DecodedImage>> textures)
{
    StopPlaying();
    _scene = std::move(scene);
    _textures = std::move(textures);
    _camera = Core::RigCamera{};
    _u = 0;
    {
        const QSignalBlocker blockerAnimations(_animations);
        const QSignalBlocker blockerSlider(_slider);
        _animations->clear();
        if (_scene != nullptr)
        {
            for (std::size_t i = 0; i < _scene->Animations.size(); ++i)
            {
                const auto &name = _scene->Animations[i].Name;
                _animations->addItem(name.empty() ? tr("animation %1").arg(i + 1) : QString::fromStdString(name), static_cast<int>(i));
            }
        }
        _animation = _animations->count() > 0 ? 0 : -1;
        _slider->setValue(0);
    }
    _controls->setVisible(_scene != nullptr && !_scene->Animations.empty());
    UpdateControls();
    _canvas->update();
}

auto RigViewer::Seconds() const -> double
{
    if (_scene == nullptr || _animation < 0 || static_cast<std::size_t>(_animation) >= _scene->Animations.size())
    {
        return 0;
    }
    return _u * _scene->Animations[static_cast<std::size_t>(_animation)].Duration;
}

auto RigViewer::CurrentFrameImage(std::uint32_t width, std::uint32_t height) const -> Core::DecodedImage
{
    const auto pose = Core::EvaluateRig(*_scene, _animation, Seconds());
    std::vector<const Core::DecodedImage *> pictures;
    pictures.reserve(_textures.size());
    for (const auto &texture : _textures)
    {
        pictures.push_back(texture.get());
    }
    return Core::RenderRig(*_scene, pose, _camera, width, height, {30, 32, 38}, pictures);
}

void RigViewer::UpdateControls()
{
    _slider->setEnabled(_animation >= 0);
    _play->setEnabled(_animation >= 0);
    if (_scene != nullptr && _animation >= 0)
    {
        const double duration = _scene->Animations[static_cast<std::size_t>(_animation)].Duration;
        _hint->setText(tr("frame %1 / %2").arg(qRound(Seconds() * 25.0)).arg(qRound(duration * 25.0)));
    }
    else
    {
        _hint->setText(tr("rest pose"));
    }
}

auto RigViewer::GetDescription() const -> QString
{
    if (_scene == nullptr)
    {
        return {};
    }
    return tr("Model: %1 limbs, %2 triangles, %3 animations").arg(_scene->Parent.size()).arg(_scene->Indices.size() / 3).arg(_scene->Animations.size());
}

void RigViewer::SelectAnimation(int index)
{
    _animations->setCurrentIndex(index);
}

void RigViewer::SetPosition(double u)
{
    _slider->setValue(qRound(u * SLIDER_STEPS));
}

void RigViewer::OnTick()
{
    if (_scene == nullptr || _animation < 0)
    {
        StopPlaying();
        return;
    }
    const double duration = std::max(_scene->Animations[static_cast<std::size_t>(_animation)].Duration, 1e-3);
    _u = std::fmod(_u + _timer->interval() / 1000.0 * _speedFactor / duration, 1.0);
    {
        const QSignalBlocker blocker(_slider);
        _slider->setValue(qRound(_u * SLIDER_STEPS));
    }
    UpdateControls();
    _canvas->update();
}

void RigViewer::StopPlaying()
{
    _timer->stop();
    _playing = false;
    _play->setText(tr("Play"));
}

void RigViewer::hideEvent(QHideEvent *event)
{
    StopPlaying(); // no animation while another preview is shown
    QWidget::hideEvent(event);
}
} // namespace Dup::Ui
