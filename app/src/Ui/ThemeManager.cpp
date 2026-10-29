// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace Dup::Ui
{
namespace
{
Theme currentTheme = Theme::DARK;

auto DarkPalette() -> QPalette
{
    QPalette palette;
    const QColor window(0x2b, 0x2b, 0x2b);
    const QColor base(0x23, 0x23, 0x23);
    const QColor text(0xd8, 0xd8, 0xd8);
    const QColor accent(0x2d, 0x7d, 0xd2);
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, QColor(0x28, 0x28, 0x28));
    palette.setColor(QPalette::ToolTipBase, QColor(0x3c, 0x3c, 0x3c));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, QColor(0x3a, 0x3a, 0x3a));
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x70, 0x70, 0x70));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x70, 0x70, 0x70));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x70, 0x70, 0x70));
    return palette;
}

auto LightPalette() -> QPalette
{
    QPalette palette;
    const QColor accent(0x1f, 0x6f, 0xc5);
    palette.setColor(QPalette::Window, QColor(0xf0, 0xf0, 0xf0));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor(0xf6, 0xf6, 0xf6));
    palette.setColor(QPalette::Button, QColor(0xe6, 0xe6, 0xe6));
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, accent);
    return palette;
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
} // namespace

void ThemeManager::Apply(Theme theme)
{
    currentTheme = theme;
    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setPalette(theme == Theme::DARK ? DarkPalette() : LightPalette());
    qApp->setStyleSheet(STYLE_SHEET);
}

auto ThemeManager::Current() -> Theme
{
    return currentTheme;
}
} // namespace Dup::Ui
