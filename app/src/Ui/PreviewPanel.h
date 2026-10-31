// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/PreviewWorker.h"

#include <QBuffer>
#include <QWidget>

#include <optional>

class QAudioOutput;
class QLabel;
class QMediaPlayer;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QStackedWidget;
class QToolButton;

namespace Dup::Ui
{
class ImageViewer;
class MeshViewer;
class ModelViewer;

/// The "Viewer" panel: shows what the preview worker prepared (image, text, hex dump, sound player or a message).
class PreviewPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit PreviewPanel(QWidget *parent = nullptr);
    ~PreviewPanel() override;

    void ShowMessage(const QString &text);
    void ShowResult(const Core::PreviewResult &result);
    [[nodiscard]] auto GetImageViewer() const -> ImageViewer *
    {
        return _image;
    }
    /// Test hook: the models are shown animated at this position (0 to 1) instead of in their rest pose.
    void SetModelPose(double u)
    {
        _modelPose = u;
    }
    /// A short description of what is shown (for the status bar and the inspector), "" when nothing.
    [[nodiscard]] auto GetDescription() const -> const QString &
    {
        return _description;
    }

  signals:
    void DescriptionChanged(const QString &description);

  private:
    void ShowAudio(const Core::PreviewResult &result);
    void StopAudio();

    QStackedWidget *_pages;
    QWidget *_imagePage; // the zoom buttons and the viewer
    ImageViewer *_image;
    MeshViewer *_mesh;
    ModelViewer *_model;
    QPlainTextEdit *_text;
    QLabel *_message;
    QWidget *_audioPage;
    QLabel *_audioInfo;
    QPushButton *_playButton;
    QSlider *_position;
    QMediaPlayer *_player;
    QAudioOutput *_output;
    QBuffer _audioBuffer;
    bool _audioLoaded{false}; // the player has a source
    QByteArray _audioBytes;
    QString _description;
    QLabel *_note;
    std::optional<double> _modelPose;
};
} // namespace Dup::Ui
