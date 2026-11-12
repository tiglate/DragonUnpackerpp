// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/ViewNavigation.h"

#include <QDialog>
#include <QGuiApplication>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>

namespace Dup::Ui
{
namespace
{
// the texts are marked with their context (QT_TRANSLATE_NOOP): lupdate cannot always tell the class of these functions
auto Tr(const char *text) -> QString
{
    return QCoreApplication::translate("ViewNavigation", text);
}

constexpr double TURN_PER_PIXEL = 0.01;
constexpr double ZOOM_STEP = 1.15;
constexpr double MIN_ZOOM = 0.1;
constexpr double MAX_ZOOM = 200.0;
constexpr double KEY_MOVE = 40.0; // pixels

// Zoom by a factor, keeping the point at `at` (pixels from the centre of the view) where it is on the screen
void ZoomAt(ViewState view, double factor, double atX, double atY)
{
    const double zoom = std::clamp(view.Zoom * factor, MIN_ZOOM, MAX_ZOOM);
    const double applied = zoom / view.Zoom;
    view.PanX = atX - (atX - view.PanX) * applied;
    view.PanY = atY - (atY - view.PanY) * applied;
    view.Zoom = zoom;
}

auto IsMoveDrag(const QMouseEvent *event) -> bool
{
    const auto buttons = event->buttons();
    return (buttons & (Qt::RightButton | Qt::MiddleButton)) != 0 || ((buttons & Qt::LeftButton) != 0 && (event->modifiers() & Qt::ShiftModifier) != 0);
}
} // namespace

void ViewNavigation::Press(QMouseEvent *event, QPoint &last)
{
    last = event->pos();
}

auto ViewNavigation::Move(QMouseEvent *event, QPoint &last, ViewState view) -> bool
{
    const QPoint delta = event->pos() - last;
    if (IsMoveDrag(event))
    {
        view.PanX += delta.x();
        view.PanY += delta.y();
    }
    else if ((event->buttons() & Qt::LeftButton) != 0)
    {
        view.Yaw += view.YawSign * delta.x() * TURN_PER_PIXEL;
        view.Pitch = std::clamp(view.Pitch + delta.y() * TURN_PER_PIXEL, -1.5, 1.5);
    }
    else
    {
        return false;
    }
    last = event->pos();
    return true;
}

auto ViewNavigation::Wheel(QWheelEvent *event, QSize size, ViewState view) -> bool
{
    event->accept();
    if (event->angleDelta().y() == 0)
    {
        return false;
    }
    const QPointF at = event->position();
    ZoomAt(view, event->angleDelta().y() > 0 ? ZOOM_STEP : 1.0 / ZOOM_STEP, at.x() - size.width() / 2.0, at.y() - size.height() / 2.0);
    return true;
}

auto ViewNavigation::Key(QKeyEvent *event, QWidget *owner, ViewState view, const std::vector<Shortcut> &viewerKeys) -> bool
{
    switch (event->key())
    {
    case Qt::Key_Left:
        view.PanX -= KEY_MOVE;
        return true;
    case Qt::Key_Right:
        view.PanX += KEY_MOVE;
        return true;
    case Qt::Key_Up:
        view.PanY -= KEY_MOVE;
        return true;
    case Qt::Key_Down:
        view.PanY += KEY_MOVE;
        return true;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        ZoomAt(view, ZOOM_STEP, 0, 0);
        return true;
    case Qt::Key_Minus:
        ZoomAt(view, 1.0 / ZOOM_STEP, 0, 0);
        return true;
    case Qt::Key_H:
    case Qt::Key_F1:
    case Qt::Key_Question:
        ShowHelp(owner, viewerKeys);
        return true;
    default:
        return false;
    }
}

auto ViewNavigation::AllViewerKeys() -> std::vector<Shortcut>
{
    // one statement per key: lupdate puts the texts of a braced list in the wrong context
    std::vector<Shortcut> keys;
    keys.emplace_back("W", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Wireframe on or off")));
    keys.emplace_back("F", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Frame the model again (the first view)")));
    keys.emplace_back("B", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Back faces shown or hidden (Outrage models)")));
    keys.emplace_back(Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Space")), Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Play or pause the animation (animated models)")));
    keys.emplace_back("L", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Lightmaps on or off (Requiem levels)")));
    return keys;
}

void ViewNavigation::ShowHelp(QWidget *parent, const std::vector<Shortcut> &viewerKeys)
{
    MakeHelpDialog(parent, viewerKeys)->exec();
}

auto ViewNavigation::MakeHelpDialog(QWidget *parent, const std::vector<Shortcut> &viewerKeys) -> std::unique_ptr<QDialog>
{
    std::vector<Shortcut> rows{{Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Left button drag")), Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Turn the model"))},
                               {Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Right or middle button drag, or Shift + left button drag")), Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Move the model"))},
                               {Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Wheel")), Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Zoom in or out at the pointer"))},
                               {Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Arrow keys")), Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Move the model"))},
                               {"+ / -", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Zoom in or out"))}};
    rows.insert(rows.end(), viewerKeys.begin(), viewerKeys.end());
    rows.emplace_back("H, F1, ?", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "This help")));
    // the columns follow the direction of the language (Arabic, Hebrew: the keys on the right)
    const bool rtl = QGuiApplication::isRightToLeft();
    const QString cell = QString("<td align='%1'>").arg(rtl ? "right" : "left");
    QString html = QString("<table cellspacing='0' cellpadding='4' dir='%1'>").arg(rtl ? "rtl" : "ltr");
    for (const auto &[key, action] : rows)
    {
        html += "<tr>" + cell + "<b>" + key.toHtmlEscaped() + "</b></td>" + cell + action.toHtmlEscaped() + "</td></tr>";
    }
    html += "</table><p>" + Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Click the view first: the keys go to the view that has the focus.")).toHtmlEscaped() + "</p>";

    auto dialog = std::make_unique<QDialog>(parent);
    dialog->setWindowTitle(Tr(QT_TRANSLATE_NOOP("ViewNavigation", "3D viewer controls")));
    auto *layout = new QVBoxLayout(dialog.get());
    auto *text = new QLabel(html, dialog.get());
    text->setTextFormat(Qt::RichText);
    layout->addWidget(text);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog.get());
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog.get(), &QDialog::reject);
    layout->addWidget(buttons);
    return dialog;
}

void ViewNavigation::DrawHint(QPainter &painter, const QRect &area, const QColor &color)
{
    const QString hint = Tr(QT_TRANSLATE_NOOP("ViewNavigation", "H: help"));
    painter.save();
    QColor faint = color;
    faint.setAlpha(150);
    painter.setPen(faint);
    painter.drawText(area.adjusted(6, 4, -6, -4), Qt::AlignRight | Qt::AlignBottom, hint);
    painter.restore();
}
} // namespace Dup::Ui
