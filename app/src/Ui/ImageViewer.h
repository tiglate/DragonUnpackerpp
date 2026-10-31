// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <QImage>
#include <QScrollArea>

class QLabel;

namespace Dup::Ui
{
/// Image view with a checkerboard background, zoom (Ctrl+wheel, +/-, 0 = 100%, F = fit) and the hand tool
/// (drag with the left button while Space is held, or with the middle button).
class ImageViewer : public QScrollArea
{
    Q_OBJECT

  public:
    explicit ImageViewer(QWidget *parent = nullptr);

    void ShowImage(const QImage &image);
    void ShowMessage(const QString &text);
    void SetZoom(double zoom);
    void FitToWindow();
    void ZoomIn();
    void ZoomOut();
    [[nodiscard]] auto HasImage() const -> bool
    {
        return !_image.isNull();
    }

  signals:
    void ZoomChanged(double zoom);

  protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

  private:
    void UpdatePixmap();
    /// Zoom by a factor keeping the point of the image under `anchor` (viewport coordinates) where it is.
    void ZoomAround(double factor, const QPointF &anchor);

    QLabel *_label;
    QImage _image;
    double _zoom{1.0};
    bool _autoFit{true}; // follows the size of the panel until the user zooms
    QPoint _dragStart;
    bool _dragging{false};
    int _wheelRemainder{0};
};
} // namespace Dup::Ui
