// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/PreviewPanel.h"

#include "Ui/ImageViewer.h"
#include "Ui/MeshViewer.h"

#include <QAudioOutput>
#include <QFontDatabase>
#include <QImage>
#include <QLabel>
#include <QMediaPlayer>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace Dup::Ui
{
namespace
{
auto FormatTime(qint64 milliseconds) -> QString
{
    const qint64 seconds = milliseconds / 1000;
    return QString("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
}
} // namespace

PreviewPanel::PreviewPanel(QWidget *parent)
    : QWidget(parent), _pages(new QStackedWidget(this)), _imagePage(new QWidget(this)), _image(new ImageViewer(_imagePage)), _mesh(new MeshViewer(this)), _text(new QPlainTextEdit(this)),
      _message(new QLabel(this)), _audioPage(new QWidget(this)), _audioInfo(new QLabel(_audioPage)), _playButton(new QPushButton(tr("Play"), _audioPage)),
      _position(new QSlider(Qt::Horizontal, _audioPage)), _player(new QMediaPlayer(this)), _output(new QAudioOutput(this)), _note(new QLabel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_pages, 1);
    layout->addWidget(_note);
    _note->setContentsMargins(6, 2, 6, 2);
    _note->setStyleSheet("color: palette(placeholder-text);");
    _note->hide();

    _text->setReadOnly(true);
    _text->setLineWrapMode(QPlainTextEdit::NoWrap);
    _text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    _message->setAlignment(Qt::AlignCenter);
    _message->setWordWrap(true);

    auto *audioLayout = new QVBoxLayout(_audioPage);
    audioLayout->addStretch(1);
    audioLayout->addWidget(_audioInfo, 0, Qt::AlignHCenter);
    audioLayout->addWidget(_position);
    audioLayout->addWidget(_playButton, 0, Qt::AlignHCenter);
    audioLayout->addStretch(1);

    _pages->addWidget(_message);
    auto *imageLayout = new QVBoxLayout(_imagePage);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setSpacing(0);
    auto *zoomBar = new QHBoxLayout();
    zoomBar->setContentsMargins(4, 2, 4, 2);
    const auto zoomButton = [&](const QString &text, const QString &tip, auto action) {
        auto *button = new QToolButton(_imagePage);
        button->setText(text);
        button->setToolTip(tip);
        button->setAutoRaise(true);
        button->setMinimumWidth(28);
        connect(button, &QToolButton::clicked, this, action);
        zoomBar->addWidget(button);
    };
    zoomButton("\u2212", tr("Zoom out (Ctrl+wheel down, -)"), [this] { _image->ZoomOut(); });
    zoomButton("+", tr("Zoom in (Ctrl+wheel up, +)"), [this] { _image->ZoomIn(); });
    zoomButton("1:1", tr("Actual size (0)"), [this] { _image->SetZoom(1.0); });
    zoomButton(tr("Fit"), tr("Fit to the panel (F)"), [this] { _image->FitToWindow(); });
    zoomBar->addStretch(1);
    imageLayout->addLayout(zoomBar);
    imageLayout->addWidget(_image, 1);
    _pages->addWidget(_imagePage);
    _pages->addWidget(_mesh);
    _pages->addWidget(_text);
    _pages->addWidget(_audioPage);

    _player->setAudioOutput(_output);
    connect(_playButton, &QPushButton::clicked, this, [this] {
        if (_player->playbackState() == QMediaPlayer::PlayingState)
        {
            _player->pause();
        }
        else
        {
            _player->play();
        }
    });
    connect(_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        _playButton->setText(state == QMediaPlayer::PlayingState ? tr("Pause") : tr("Play"));
    });
    connect(_player, &QMediaPlayer::durationChanged, this, [this](qint64 duration) { _position->setRange(0, static_cast<int>(duration)); });
    connect(_player, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (!_position->isSliderDown())
        {
            _position->setValue(static_cast<int>(position));
        }
        _audioInfo->setText(tr("%1 / %2").arg(FormatTime(position), FormatTime(_player->duration())));
    });
    connect(_position, &QSlider::sliderMoved, this, [this](int value) { _player->setPosition(value); });
    connect(_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &text) {
        _audioInfo->setText(tr("Cannot play this sound: %1").arg(text));
    });
    ShowMessage(tr("Select an entry to preview it"));
}

PreviewPanel::~PreviewPanel()
{
    StopAudio();
}

void PreviewPanel::StopAudio()
{
    _player->stop();
    _player->setSourceDevice(nullptr);
    if (_audioBuffer.isOpen())
    {
        _audioBuffer.close();
    }
}

void PreviewPanel::ShowMessage(const QString &text)
{
    StopAudio();
    _message->setText(text);
    _pages->setCurrentWidget(_message);
    _note->hide();
    _description.clear();
    emit DescriptionChanged(_description);
}

void PreviewPanel::ShowAudio(const Core::PreviewResult &result)
{
    StopAudio();
    _audioBytes = QByteArray(reinterpret_cast<const char *>(result.Bytes.data()), static_cast<qsizetype>(result.Bytes.size()));
    _audioBuffer.setData(_audioBytes);
    _audioBuffer.open(QIODevice::ReadOnly);
    _player->setSourceDevice(&_audioBuffer, QUrl("file:///preview." + QString::fromStdString(result.Extension)));
    _position->setValue(0);
    _audioInfo->setText(tr("Sound (%1, %2)").arg(QString::fromStdString(result.Extension).toUpper(), QString::number(result.Bytes.size() / 1024) + " KB"));
    _pages->setCurrentWidget(_audioPage);
}

void PreviewPanel::ShowResult(const Core::PreviewResult &result)
{
    _description.clear();
    switch (result.Kind)
    {
    case Core::PreviewKind::IMAGE: {
        StopAudio();
        const QImage image(result.Image.Rgba.data(), static_cast<int>(result.Image.Width), static_cast<int>(result.Image.Height),
                           static_cast<qsizetype>(result.Image.Width) * 4, QImage::Format_RGBA8888);
        _image->ShowImage(image.copy());
        _pages->setCurrentWidget(_imagePage);
        _description = tr("Image %1 x %2").arg(result.Image.Width).arg(result.Image.Height);
        break;
    }
    case Core::PreviewKind::IMAGE_FILE: {
        StopAudio();
        QImage image;
        if (image.loadFromData(reinterpret_cast<const uchar *>(result.Bytes.data()), static_cast<int>(result.Bytes.size()), result.Extension.c_str()))
        {
            _image->ShowImage(image);
            _pages->setCurrentWidget(_imagePage);
            _description = tr("Image %1 x %2").arg(image.width()).arg(image.height());
        }
        else
        {
            ShowMessage(tr("Cannot decode the image"));
            return;
        }
        break;
    }
    case Core::PreviewKind::MESH:
        StopAudio();
        _mesh->SetMesh(result.Geometry);
        _pages->setCurrentWidget(_mesh);
        _description = _mesh->GetDescription();
        break;
    case Core::PreviewKind::TEXT:
        StopAudio();
        _text->setPlainText(QString::fromUtf8(result.Text.data(), static_cast<qsizetype>(result.Text.size())));
        _pages->setCurrentWidget(_text);
        _description = tr("Text");
        break;
    case Core::PreviewKind::HEX:
        StopAudio();
        _text->setPlainText(QString::fromStdString(result.Text));
        _pages->setCurrentWidget(_text);
        _description = tr("Hex dump");
        break;
    case Core::PreviewKind::AUDIO:
        ShowAudio(result);
        _description = tr("Sound");
        break;
    case Core::PreviewKind::ERROR_MESSAGE:
        ShowMessage(tr("Cannot preview: %1").arg(QString::fromStdString(result.Message)));
        return;
    case Core::PreviewKind::NONE:
        ShowMessage(result.Message.empty() ? tr("No preview") : tr("No preview (%1)").arg(QString::fromStdString(result.Message)));
        return;
    }
    if (result.Message.empty())
    {
        _note->hide();
    }
    else
    {
        _note->setText(QString::fromStdString(result.Message));
        _note->show();
    }
    emit DescriptionChanged(_description);
}
} // namespace Dup::Ui
