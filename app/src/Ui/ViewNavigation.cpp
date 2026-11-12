// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/ViewNavigation.h"

#include "Ui/Icons.h"

#include <QAction>
#include <QDialog>
#include <QEvent>
#include <QToolButton>
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
constexpr double BUTTON_TURN = 0.15; // radians, about 9 degrees

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

auto ViewNavigation::Wheel(QWheelEvent *event, const QRect &area, ViewState view) -> bool
{
    event->accept();
    if (event->angleDelta().y() == 0)
    {
        return false;
    }
    const QPointF at = event->position();
    const QPointF centre = QRectF(area).center();
    ZoomAt(view, event->angleDelta().y() > 0 ? ZOOM_STEP : 1.0 / ZOOM_STEP, at.x() - centre.x(), at.y() - centre.y());
    return true;
}

void ViewNavigation::TurnStep(ViewState view, int right, int down)
{
    view.Yaw += view.YawSign * right * BUTTON_TURN;
    view.Pitch = std::clamp(view.Pitch + down * BUTTON_TURN, -1.5, 1.5);
}

void ViewNavigation::MoveStep(ViewState view, int right, int down)
{
    view.PanX += right * KEY_MOVE;
    view.PanY += down * KEY_MOVE;
}

void ViewNavigation::ZoomStep(ViewState view, int in)
{
    ZoomAt(view, in > 0 ? ZOOM_STEP : 1.0 / ZOOM_STEP, 0, 0);
}

auto ViewNavigation::Key(QKeyEvent *event, QWidget *owner, ViewState view, const std::vector<Shortcut> &viewerKeys) -> bool
{
    switch (event->key())
    {
    case Qt::Key_Left:
        MoveStep(view, -1, 0);
        return true;
    case Qt::Key_Right:
        MoveStep(view, 1, 0);
        return true;
    case Qt::Key_Up:
        MoveStep(view, 0, -1);
        return true;
    case Qt::Key_Down:
        MoveStep(view, 0, 1);
        return true;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        ZoomStep(view, 1);
        return true;
    case Qt::Key_Minus:
        ZoomStep(view, -1);
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
    keys.emplace_back("[ / ]", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Lightmaps darker or brighter (Requiem levels)")));
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

ViewToolBar::ViewToolBar(QWidget *parent, std::function<ViewState()> view, std::function<void()> changed, std::function<void()> frame,
                         std::vector<Switch> switches, std::vector<Command> commands, std::vector<ViewNavigation::Shortcut> helpKeys)
    : QToolBar(parent), _changed(std::move(changed))
{
    setIconSize(QSize(18, 18));
    setMovable(false);
    setFloatable(false);
    setFocusPolicy(Qt::NoFocus);
    setContextMenuPolicy(Qt::PreventContextMenu);
    const auto step = [this, view](auto apply) {
        return [this, view, apply] {
            apply(view());
            _changed();
        };
    };
    const QString drag = Tr(QT_TRANSLATE_NOOP("ViewNavigation", "left button drag"));
    AddButton("turn-left", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Turn left")), drag, step([](ViewState v) { ViewNavigation::TurnStep(v, -1, 0); }), true);
    AddButton("turn-right", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Turn right")), drag, step([](ViewState v) { ViewNavigation::TurnStep(v, 1, 0); }), true);
    AddButton("tilt-up", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Tilt up")), drag, step([](ViewState v) { ViewNavigation::TurnStep(v, 0, -1); }), true);
    AddButton("tilt-down", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Tilt down")), drag, step([](ViewState v) { ViewNavigation::TurnStep(v, 0, 1); }), true);
    addSeparator();
    AddButton("move-left", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Move left")), Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Left arrow")),
              step([](ViewState v) { ViewNavigation::MoveStep(v, -1, 0); }), true);
    AddButton("move-right", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Move right")), Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Right arrow")),
              step([](ViewState v) { ViewNavigation::MoveStep(v, 1, 0); }), true);
    AddButton("move-up", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Move up")), Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Up arrow")),
              step([](ViewState v) { ViewNavigation::MoveStep(v, 0, -1); }), true);
    AddButton("move-down", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Move down")), Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Down arrow")),
              step([](ViewState v) { ViewNavigation::MoveStep(v, 0, 1); }), true);
    addSeparator();
    AddButton("zoom-in", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Zoom in")), "+", step([](ViewState v) { ViewNavigation::ZoomStep(v, 1); }), true);
    AddButton("zoom-out", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Zoom out")), "-", step([](ViewState v) { ViewNavigation::ZoomStep(v, -1); }), true);
    AddButton("frame", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Frame the model again")), "F", [this, frame] {
        frame();
        _changed();
    }, false);
    if (!switches.empty())
    {
        addSeparator();
    }
    for (auto &item : switches)
    {
        AddButton(item.Icon, item.Text, item.Key, [this, flip = item.Flip] {
            flip();
            _changed();
            Refresh();
        }, false);
        actions().back()->setCheckable(true);
        _checks.emplace_back(actions().back(), item.IsOn);
    }
    if (!commands.empty())
    {
        addSeparator();
    }
    for (auto &item : commands)
    {
        AddButton(item.Icon, item.Text, item.Key, [this, run = item.Run] {
            run();
            _changed();
        }, true);
    }
    addSeparator();
    AddButton("help", Tr(QT_TRANSLATE_NOOP("ViewNavigation", "Help of the controls")), "H", [this, helpKeys] { ViewNavigation::ShowHelp(this, helpKeys); }, false);
    RefreshIcons();
    Refresh();
}

void ViewToolBar::AddButton(const QString &icon, const QString &text, const QString &key, std::function<void()> run, bool repeat)
{
    auto *action = addAction(text);
    action->setToolTip(key.isEmpty() ? text : QString("%1 (%2)").arg(text, key));
    connect(action, &QAction::triggered, this, [run = std::move(run)] { run(); });
    if (auto *button = qobject_cast<QToolButton *>(widgetForAction(action)))
    {
        button->setFocusPolicy(Qt::NoFocus);
        button->setAutoRaise(true);
        button->setAutoRepeat(repeat); // held down: turns, moves or zooms on
        button->setObjectName("view-" + icon);
    }
    _icons.emplace_back(action, icon);
}

void ViewToolBar::Refresh()
{
    for (const auto &[action, isOn] : _checks)
    {
        action->setChecked(isOn());
    }
}

void ViewToolBar::RefreshIcons()
{
    const QColor colour = palette().color(QPalette::ButtonText);
    for (const auto &[action, icon] : _icons)
    {
        action->setIcon(Icons::Make(icon, colour));
    }
}

void ViewToolBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange)
    {
        RefreshIcons(); // the theme changed
    }
    QToolBar::changeEvent(event);
}
} // namespace Dup::Ui
