// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/ModelViewer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
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
constexpr double LOOP_SECONDS = 4.0; // one pass of the animation slider while playing
} // namespace

/// The drawing surface: renders the scene with the current pose and camera on every paint.
class ModelCanvas : public QWidget
{
  public:
    explicit ModelCanvas(ModelViewer *owner) : QWidget(owner), _owner(owner)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(120, 120);
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        // dark whatever the theme: the glows add their light to the background
        const QColor background(30, 32, 38);
        painter.fillRect(rect(), background);
        if (_owner->_scene == nullptr || width() < 2 || height() < 2)
        {
            return;
        }
        const auto image = Core::RenderOof(*_owner->_scene, _owner->CurrentPose(), _owner->_camera, static_cast<std::uint32_t>(width()),
                                           static_cast<std::uint32_t>(height()),
                                           {static_cast<std::uint8_t>(background.red()), static_cast<std::uint8_t>(background.green()), static_cast<std::uint8_t>(background.blue())});
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
            camera.Yaw += (event->pos().x() - _last.x()) * 0.01;
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
        else if (event->key() == Qt::Key_B)
        {
            camera.CullBackFaces = !camera.CullBackFaces;
        }
        else if (event->key() == Qt::Key_F)
        {
            camera = Core::OofCamera{};
        }
        else
        {
            QWidget::keyPressEvent(event);
            return;
        }
        update();
    }

  private:
    ModelViewer *_owner;
    QPoint _last;
};

ModelViewer::ModelViewer(QWidget *parent)
    : QWidget(parent), _canvas(new ModelCanvas(this)), _controls(new QWidget(this)), _animate(new QCheckBox(tr("Animate"), _controls)),
      _play(new QPushButton(tr("Play"), _controls)), _slider(new QSlider(Qt::Horizontal, _controls)), _speed(new QComboBox(_controls)), _hint(new QLabel(_controls)), _timer(new QTimer(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_canvas, 1);
    layout->addWidget(_controls);
    auto *row = new QHBoxLayout(_controls);
    row->setContentsMargins(6, 2, 6, 2);
    row->addWidget(_animate);
    row->addWidget(_play);
    row->addWidget(_slider, 1);
    row->addWidget(_speed);
    row->addWidget(_hint);
    _slider->setRange(0, SLIDER_STEPS);
    _animate->setToolTip(tr("Off: the model as it is drawn without animation. On: the animation tracks at the position of the slider."));
    _hint->setText(tr("rest pose"));
    for (const double factor : {0.1, 0.25, 0.5, 1.0, 2.0, 4.0})
    {
        _speed->addItem(QString("%1x").arg(factor), factor);
    }
    _speed->setCurrentIndex(3);
    _speed->setToolTip(tr("Speed of the playback (one pass of the animation takes 4 seconds at 1x; the $rotate parts follow it)"));
    _timer->setInterval(33);
    _controls->hide();
    UpdateControls();

    connect(_animate, &QCheckBox::toggled, this, [this](bool on) {
        _rest = !on;
        if (!on)
        {
            StopPlaying();
        }
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
        _animate->setChecked(true);
        _playing = true;
        _play->setText(tr("Pause"));
        _timer->start();
    });
    connect(_speed, &QComboBox::currentIndexChanged, this, [this](int row) { _speedFactor = _speed->itemData(row).toDouble(); });
    connect(_timer, &QTimer::timeout, this, [this] { OnTick(); });
}

ModelViewer::~ModelViewer() = default;

void ModelViewer::SetScene(std::shared_ptr<const Core::OofScene> scene)
{
    StopPlaying();
    _scene = std::move(scene);
    _camera = Core::OofCamera{};
    _rest = true;
    _u = 0;
    _seconds = 0;
    {
        const QSignalBlocker blocker(_animate);
        _animate->setChecked(false);
    }
    {
        const QSignalBlocker blocker(_slider);
        _slider->setValue(0);
    }
    _controls->setVisible(_scene != nullptr && _scene->Animated);
    UpdateControls();
    _canvas->update();
}

void ModelViewer::UpdateControls()
{
    _slider->setEnabled(!_rest);
    _play->setEnabled(_scene != nullptr);
    _hint->setText(_rest ? tr("rest pose") : tr("%1%").arg(qRound(_u * 100)));
}

auto ModelViewer::GetDescription() const -> QString
{
    if (_scene == nullptr)
    {
        return {};
    }
    return tr("Model: %1 subobjects, %2 triangles").arg(_scene->Model->Subobjects.size()).arg(_scene->Triangles);
}

auto ModelViewer::CurrentPose() const -> Core::OofPose
{
    Core::OofPoseOptions options;
    options.Rest = _rest;
    options.U = _u;
    options.Seconds = _seconds;
    return Core::EvaluateOofPose(*_scene->Model, options);
}

void ModelViewer::SetAnimation(bool animated, double u)
{
    _animate->setChecked(animated);
    _slider->setValue(qRound(u * SLIDER_STEPS));
}

auto ModelViewer::GetPoseNotes() const -> QStringList
{
    QStringList notes;
    if (_scene != nullptr)
    {
        for (const auto &note : CurrentPose().Notes)
        {
            notes << QString::fromStdString(note);
        }
    }
    return notes;
}

void ModelViewer::OnTick()
{
    _seconds += _timer->interval() / 1000.0 * _speedFactor;
    _u = std::fmod(_seconds / LOOP_SECONDS, 1.0);
    {
        const QSignalBlocker blocker(_slider);
        _slider->setValue(qRound(_u * SLIDER_STEPS));
    }
    UpdateControls();
    _canvas->update();
}

void ModelViewer::StopPlaying()
{
    _timer->stop();
    _playing = false;
    _play->setText(tr("Play"));
}

void ModelViewer::hideEvent(QHideEvent *event)
{
    StopPlaying(); // no animation while another preview is shown
    QWidget::hideEvent(event);
}
} // namespace Dup::Ui
