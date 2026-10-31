// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/ImageViewer.h"

#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace Dup::Ui
{
namespace
{
auto Checkerboard(const QSize &size) -> QPixmap
{
    QPixmap pixmap(size);
    QPainter painter(&pixmap);
    constexpr int CELL = 8;
    for (int y = 0; y < size.height(); y += CELL)
    {
        for (int x = 0; x < size.width(); x += CELL)
        {
            painter.fillRect(x, y, CELL, CELL, ((x / CELL + y / CELL) % 2 == 0) ? QColor(0x55, 0x55, 0x55) : QColor(0x44, 0x44, 0x44));
        }
    }
    return pixmap;
}
} // namespace

ImageViewer::ImageViewer(QWidget *parent) : QScrollArea(parent), _label(new QLabel(this))
{
    _label->setAlignment(Qt::AlignCenter);
    _label->setMargin(0);
    setWidget(_label);
    setAlignment(Qt::AlignCenter);
    setFocusPolicy(Qt::StrongFocus);
    ShowMessage(tr("Select an image to preview it"));
}

void ImageViewer::ShowMessage(const QString &text)
{
    _image = QImage();
    _label->setPixmap(QPixmap());
    _label->setText(text);
    _label->adjustSize();
}

void ImageViewer::ShowImage(const QImage &image)
{
    _image = image;
    FitToWindow();
}

void ImageViewer::SetZoom(double zoom)
{
    _autoFit = false; // a zoom chosen by the user (FitToWindow sets the flag again)
    _zoom = std::clamp(zoom, 0.05, 32.0);
    UpdatePixmap();
    emit ZoomChanged(_zoom);
}

void ImageViewer::FitToWindow()
{
    if (_image.isNull())
    {
        return;
    }
    const double fitX = static_cast<double>(viewport()->width() - 4) / _image.width();
    const double fitY = static_cast<double>(viewport()->height() - 4) / _image.height();
    double zoom = std::min({8.0, fitX, fitY});
    if (zoom > 1.0)
    {
        zoom = std::floor(zoom); // small images are enlarged by whole factors so that pixels stay sharp
    }
    SetZoom(zoom);
    _autoFit = true;
}

void ImageViewer::UpdatePixmap()
{
    if (_image.isNull())
    {
        return;
    }
    const QSize size(std::max(1, static_cast<int>(_image.width() * _zoom)), std::max(1, static_cast<int>(_image.height() * _zoom)));
    QPixmap canvas = Checkerboard(size);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, _zoom < 1.0);
    painter.drawImage(QRect(QPoint(0, 0), size), _image);
    painter.end();
    _label->setPixmap(canvas);
    _label->resize(size);
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    if (_autoFit && !_image.isNull())
    {
        FitToWindow();
    }
}

void ImageViewer::ZoomIn()
{
    ZoomAround(1.25, QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0));
}

void ImageViewer::ZoomOut()
{
    ZoomAround(0.8, QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0));
}

void ImageViewer::ZoomAround(double factor, const QPointF &anchor)
{
    if (_image.isNull())
    {
        return;
    }
    // the point of the image under the anchor, before and after the change
    const QPointF label = _label->mapFrom(viewport(), anchor.toPoint());
    const double before = _zoom;
    SetZoom(_zoom * factor);
    const double ratio = _zoom / before;
    const QPointF moved = _label->mapFrom(viewport(), anchor.toPoint());
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + static_cast<int>(label.x() * ratio - moved.x()));
    verticalScrollBar()->setValue(verticalScrollBar()->value() + static_cast<int>(label.y() * ratio - moved.y()));
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) != 0 && !_image.isNull())
    {
        // mouse wheels send 120 per notch, touchpads many small values: one step per 120 units
        const int delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->pixelDelta().y();
        _wheelRemainder += delta;
        const QPointF anchor = event->position();
        while (_wheelRemainder >= 120)
        {
            ZoomAround(1.25, anchor);
            _wheelRemainder -= 120;
        }
        while (_wheelRemainder <= -120)
        {
            ZoomAround(0.8, anchor);
            _wheelRemainder += 120;
        }
        event->accept();
        return;
    }
    QScrollArea::wheelEvent(event);
}

void ImageViewer::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        ZoomIn();
        break;
    case Qt::Key_Minus:
        ZoomOut();
        break;
    case Qt::Key_0:
        SetZoom(1.0);
        break;
    case Qt::Key_F:
        FitToWindow();
        break;
    case Qt::Key_Space:
        setCursor(Qt::OpenHandCursor);
        break;
    default:
        QScrollArea::keyPressEvent(event);
        return;
    }
    event->accept();
}

void ImageViewer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && cursor().shape() == Qt::OpenHandCursor))
    {
        _dragging = true;
        _dragStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QScrollArea::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (_dragging)
    {
        const QPoint delta = event->pos() - _dragStart;
        _dragStart = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QScrollArea::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if (_dragging)
    {
        _dragging = false;
        unsetCursor();
        event->accept();
        return;
    }
    QScrollArea::mouseReleaseEvent(event);
}
} // namespace Dup::Ui
