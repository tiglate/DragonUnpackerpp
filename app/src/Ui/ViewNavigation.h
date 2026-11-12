// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <QCoreApplication>
#include <QPoint>
#include <QToolBar>
#include <QSize>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

class QColor;
class QDialog;
class QKeyEvent;
class QMouseEvent;
class QPainter;
class QRect;
class QWheelEvent;
class QWidget;

namespace Dup::Ui
{
/// The view of a 3D viewer that the mouse and the keys move: the fields of its camera.
struct ViewState
{
    double &Yaw;
    double &Pitch;
    double &Zoom;
    double &PanX; // pixels
    double &PanY;
    double YawSign{1.0}; // the direction a drag to the right turns the model (the renderers differ)
};

/// The mouse and keys the 3D viewers share (the model viewers of the preview): drag with the left button to turn, with the
/// right or middle button (or Shift and the left button) to move, wheel to zoom at the pointer, arrows to move, + and - to
/// zoom, H, F1 or ? for the help. Each viewer keeps its own keys (W, F...) and lists them in the help.
class ViewNavigation
{
  public:
    /// A key and what it does, for the help.
    using Shortcut = std::pair<QString, QString>;

    static void Press(QMouseEvent *event, QPoint &last);
    /// True when the view changed.
    [[nodiscard]] static auto Move(QMouseEvent *event, QPoint &last, ViewState view) -> bool;
    /// area: the part of the widget the view is drawn in (the zoom keeps the point under the pointer in place)
    [[nodiscard]] static auto Wheel(QWheelEvent *event, const QRect &area, ViewState view) -> bool;
    /// The steps of the keys and the buttons
    static void TurnStep(ViewState view, int right, int down);
    static void MoveStep(ViewState view, int right, int down);
    static void ZoomStep(ViewState view, int in);
    /// Arrows, + and -, and the help keys (the help is shown over owner, with the keys of that viewer); true when the key was used.
    [[nodiscard]] static auto Key(QKeyEvent *event, QWidget *owner, ViewState view, const std::vector<Shortcut> &viewerKeys) -> bool;

    /// The help: the shared controls, then the keys of the viewer (shown modal).
    static void ShowHelp(QWidget *parent, const std::vector<Shortcut> &viewerKeys);
    /// The dialog of the help, not shown.
    [[nodiscard]] static auto MakeHelpDialog(QWidget *parent, const std::vector<Shortcut> &viewerKeys) -> std::unique_ptr<QDialog>;
    /// The keys of every viewer, for the help of the menu (each says the viewers it is for).
    [[nodiscard]] static auto AllViewerKeys() -> std::vector<Shortcut>;
    /// A discreet "H: help" in the corner of the view, in a colour that shows on its background.
    static void DrawHint(QPainter &painter, const QRect &area, const QColor &color);
};

/// The buttons of a 3D viewer (flat icons, every key of the viewer has one): turn, move, zoom, frame again, the switches of
/// the viewer (wireframe...), its other commands and the help. The buttons do not take the focus: the keys stay with the view.
class ViewToolBar : public QToolBar
{
  public:
    /// A switch (W, B, L...): checked when IsOn says so
    struct Switch
    {
        QString Icon;
        QString Text;
        QString Key;
        std::function<bool()> IsOn;
        std::function<void()> Flip;
    };
    /// Another command of the viewer ([ and ]...)
    struct Command
    {
        QString Icon;
        QString Text;
        QString Key;
        std::function<void()> Run;
    };

    /// view: the camera fields; changed: called after every change (redraw); frame: F; helpKeys: the keys of the viewer
    ViewToolBar(QWidget *parent, std::function<ViewState()> view, std::function<void()> changed, std::function<void()> frame,
                std::vector<Switch> switches, std::vector<Command> commands, std::vector<ViewNavigation::Shortcut> helpKeys);
    /// The checked states from the viewer (after a key changed one)
    void Refresh();

  protected:
    void changeEvent(QEvent *event) override;

  private:
    void AddButton(const QString &icon, const QString &text, const QString &key, std::function<void()> run, bool repeat);
    void RefreshIcons();

    std::function<void()> _changed;
    std::vector<std::pair<QAction *, QString>> _icons;
    std::vector<std::pair<QAction *, std::function<bool()>>> _checks;
};
} // namespace Dup::Ui
