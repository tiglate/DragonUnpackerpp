// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

#ifdef DUP_HAVE_DBUS
#include "Ui/DesktopScheme.h"
#endif

namespace Dup::Ui
{
namespace
{
Theme chosenTheme = Theme::SYSTEM;
std::optional<Theme> forcedTheme;
bool shownDark = true;
// The palette of the platform before this program replaced it (with Qt 6.4 it is the only sign of a dark desktop)
std::optional<QPalette> systemPalette;

struct Colors
{
    QColor Window, WindowText, Base, AlternateBase, Text, Button, ButtonText, ToolTipBase, ToolTipText, Placeholder;
    QColor Light, Midlight, Mid, Dark, Shadow;
    QColor Accent, Link, LinkVisited, Disabled, DisabledBase;
};

// Every role of every group is set: nothing is left to the palette of the platform.
auto MakePalette(const Colors &c) -> QPalette
{
    QPalette palette(c.ButtonText, c.Button, c.Light, c.Dark, c.Mid, c.Text, Qt::white, c.Base, c.Window);
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
    {
        palette.setColor(group, QPalette::WindowText, c.WindowText);
        palette.setColor(group, QPalette::AlternateBase, c.AlternateBase);
        palette.setColor(group, QPalette::ToolTipBase, c.ToolTipBase);
        palette.setColor(group, QPalette::ToolTipText, c.ToolTipText);
        palette.setColor(group, QPalette::PlaceholderText, c.Placeholder);
        palette.setColor(group, QPalette::Midlight, c.Midlight);
        palette.setColor(group, QPalette::Shadow, c.Shadow);
        palette.setColor(group, QPalette::Highlight, c.Accent);
        palette.setColor(group, QPalette::HighlightedText, Qt::white);
        palette.setColor(group, QPalette::Link, c.Link);
        palette.setColor(group, QPalette::LinkVisited, c.LinkVisited);
    }
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText})
    {
        palette.setColor(QPalette::Disabled, role, c.Disabled);
    }
    palette.setColor(QPalette::Disabled, QPalette::Base, c.DisabledBase);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, c.Mid);
    return palette;
}

auto DarkPalette() -> QPalette
{
    Colors c;
    c.Window = QColor(0x2b, 0x2b, 0x2b);
    c.WindowText = c.Text = c.ButtonText = c.ToolTipText = QColor(0xd8, 0xd8, 0xd8);
    c.Base = QColor(0x23, 0x23, 0x23);
    c.AlternateBase = QColor(0x28, 0x28, 0x28);
    c.Button = QColor(0x3a, 0x3a, 0x3a);
    c.ToolTipBase = QColor(0x3c, 0x3c, 0x3c);
    c.Placeholder = QColor(0x80, 0x80, 0x80);
    c.Light = QColor(0x50, 0x50, 0x50);
    c.Midlight = QColor(0x44, 0x44, 0x44);
    c.Mid = QColor(0x3f, 0x3f, 0x3f);
    c.Dark = QColor(0x1c, 0x1c, 0x1c);
    c.Shadow = QColor(0x10, 0x10, 0x10);
    c.Accent = QColor(0x2d, 0x7d, 0xd2);
    c.Link = QColor(0x5a, 0xa2, 0xf0);
    c.LinkVisited = QColor(0xa0, 0x8c, 0xe6);
    c.Disabled = QColor(0x70, 0x70, 0x70);
    c.DisabledBase = c.Window;
    return MakePalette(c);
}

auto LightPalette() -> QPalette
{
    Colors c;
    c.Window = QColor(0xf0, 0xf0, 0xf0);
    c.WindowText = c.Text = c.ButtonText = c.ToolTipText = QColor(0x1e, 0x1e, 0x1e);
    c.Base = Qt::white;
    c.AlternateBase = QColor(0xf6, 0xf6, 0xf6);
    c.Button = QColor(0xe6, 0xe6, 0xe6);
    c.ToolTipBase = QColor(0xff, 0xff, 0xdc);
    c.Placeholder = QColor(0x80, 0x80, 0x80);
    c.Light = Qt::white;
    c.Midlight = QColor(0xe3, 0xe3, 0xe3);
    c.Mid = QColor(0xc4, 0xc4, 0xc4);
    c.Dark = QColor(0xa0, 0xa0, 0xa0);
    c.Shadow = QColor(0x69, 0x69, 0x69);
    c.Accent = QColor(0x1f, 0x6f, 0xc5);
    c.Link = c.Accent;
    c.LinkVisited = QColor(0x6a, 0x3d, 0xb0);
    c.Disabled = QColor(0x9a, 0x9a, 0x9a);
    c.DisabledBase = c.Window;
    return MakePalette(c);
}

void Show();

// The preference of the desktop from the freedesktop settings portal (nullopt: none, or no portal)
std::optional<bool> portalDark;

void WatchPortal()
{
#ifdef DUP_HAVE_DBUS
    portalDark = WatchDesktopScheme([](std::optional<bool> dark) {
        portalDark = dark;
        if (forcedTheme.value_or(chosenTheme) == Theme::SYSTEM)
        {
            Show();
        }
    });
#endif
}

// Is the desktop dark? Qt 6.5 says so; else the settings portal; else the platform's palette shows it (text lighter
// than the window).
auto SystemIsDark() -> bool
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (const auto scheme = QGuiApplication::styleHints()->colorScheme(); scheme != Qt::ColorScheme::Unknown)
    {
        return scheme == Qt::ColorScheme::Dark;
    }
#endif
    if (portalDark)
    {
        return *portalDark;
    }
    if (!systemPalette)
    {
        return true; // no platform palette was seen: the default of the program
    }
    return systemPalette->color(QPalette::WindowText).lightness() > systemPalette->color(QPalette::Window).lightness();
}

// Flat details: thin separators, compact docks and headers.
const char *STYLE_SHEET = R"(
QMainWindow::separator { width: 1px; height: 1px; }
QDockWidget { font-weight: normal; }
QDockWidget::title { padding: 4px 6px; background: palette(button); }
QHeaderView::section { padding: 3px 6px; border: 0; border-right: 1px solid palette(mid); background: palette(button); }
QTableView, QTreeView, QPlainTextEdit, QScrollArea { border: 1px solid palette(mid); }
QStatusBar::item { border: 0; }
)";

void Show()
{
    const Theme theme = forcedTheme.value_or(chosenTheme);
    shownDark = theme == Theme::SYSTEM ? SystemIsDark() : theme == Theme::DARK;
    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setPalette(shownDark ? DarkPalette() : LightPalette());
    qApp->setStyleSheet(STYLE_SHEET);
}

void Initialize()
{
    if (systemPalette)
    {
        return;
    }
    systemPalette = QGuiApplication::palette(); // before the first setPalette
    WatchPortal();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, qApp, [] {
        if (forcedTheme.value_or(chosenTheme) == Theme::SYSTEM)
        {
            Show();
        }
    });
#endif
}
} // namespace

void ThemeManager::Apply(Theme theme)
{
    Initialize();
    chosenTheme = theme;
    Show();
}

void ThemeManager::Force(std::optional<Theme> theme)
{
    forcedTheme = theme;
}

auto ThemeManager::Current() -> Theme
{
    return chosenTheme;
}

auto ThemeManager::IsDark() -> bool
{
    return shownDark;
}

auto ThemeManager::FromName(const std::string &name) -> Theme
{
    return name == "dark" ? Theme::DARK : name == "light" ? Theme::LIGHT : Theme::SYSTEM;
}

auto ThemeManager::Name(Theme theme) -> std::string
{
    return theme == Theme::DARK ? "dark" : theme == Theme::LIGHT ? "light" : "system";
}
} // namespace Dup::Ui
