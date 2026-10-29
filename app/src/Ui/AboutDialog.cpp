// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/AboutDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QSysInfo>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#ifndef DUP_VERSION
#define DUP_VERSION "6.0.0"
#endif

namespace Dup::Ui
{
namespace
{
auto MakePage(const QString &html, QWidget *parent) -> QTextBrowser *
{
    auto *page = new QTextBrowser(parent);
    page->setOpenExternalLinks(true);
    page->setFrameShape(QFrame::NoFrame);
    page->setHtml(html);
    return page;
}

auto Link(const QString &url, const QString &text = {}) -> QString
{
    return QString("<a href=\"%1\">%2</a>").arg(url, text.isEmpty() ? url : text);
}
} // namespace

AboutDialog::AboutDialog(const QString &driverText, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("About"));
    setMinimumSize(560, 520);

    // The banner keeps the colors of the icon in both themes
    auto *banner = new QWidget(this);
    banner->setObjectName("aboutBanner");
    banner->setStyleSheet("#aboutBanner { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #262f57, stop:1 #0a0d1c); }"
                          "#aboutBanner QLabel { color: #f3f5ff; background: transparent; }");
    auto *bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(24, 20, 24, 20);
    bannerLayout->setSpacing(20);

    auto *logo = new QLabel(banner);
    logo->setPixmap(QIcon(":/icons/dragonunpacker.svg").pixmap(96, 96));
    bannerLayout->addWidget(logo, 0, Qt::AlignTop);

    auto *titles = new QVBoxLayout;
    auto *title = new QLabel("Dragon UnPACKer<span style=\"color:#ffb44d\">++</span>", banner);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 2.1);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setTextFormat(Qt::RichText);
    auto *tagline = new QLabel(tr("Explore, preview and extract the resources of games"), banner);
    tagline->setWordWrap(true);
    auto *version = new QLabel(tr("Version %1").arg(QString::fromLatin1(DUP_VERSION)), banner);
    version->setStyleSheet("color: #7ff0e0;");
    titles->addStretch();
    titles->addWidget(title);
    titles->addWidget(tagline);
    titles->addSpacing(6);
    titles->addWidget(version);
    titles->addStretch();
    bannerLayout->addLayout(titles, 1);

    auto *tabs = new QTabWidget(this);

    const QString site = "https://www.elberethzone.net/dragon-unpacker.html";
    tabs->addTab(MakePage(QString("<h3>Dragon UnPACKer++</h3>"
                                  "<p>%1</p>"
                                  "<p>%2</p>"
                                  "<p>%3</p>")
                              .arg(tr("A fast explorer and extractor for the resource files of games: it opens the archives, shows what is inside, "
                                      "previews pictures, sounds, text and 3D models, and extracts them, converting what is needed."),
                                   tr("It continues Dragon UnPACKer, which Alexandre \"Elbereth\" Devilliers created and developed for many years: "
                                      "the same idea and the same formats, now in C++ with a Qt 6 interface, running on Linux, and with many more formats."),
                                   tr("Original project: %1").arg(Link(site))),
                          this),
                 tr("About"));

    tabs->addTab(MakePage(QString("<h3>%1</h3>"
                                  "<p><b>Alexandre \"Elbereth\" Devilliers</b><br>%2<br>%3</p>"
                                  "<h3>%4</h3>"
                                  "<p><b>Tiglate Pileser III</b><br>%5</p>"
                                  "<p>%6</p>")
                              .arg(tr("Dragon UnPACKer: the original author"),
                                   tr("Created Dragon UnPACKer and wrote its versions 1 to 5 in Delphi, with the plugins, the formats and the HyperRipper "
                                      "that this program is built on."),
                                   Link(site), tr("Dragon UnPACKer++: port and new features"),
                                   tr("Ported the program to C++20 and Qt 6 and the formats to a new driver, and added the new formats, the Unity, FMOD and XNA "
                                      "readers, the previews, the file type identification, the translations and the other new features."),
                                   tr("Thanks to everyone who contributed formats, plugins, translations and bug reports to Dragon UnPACKer over the years.")),
                          this),
                 tr("Credits"));

    QString components = QString("<h3>%1</h3><ul>"
                                 "<li>Qt %2 (%3)</li>"
                                 "<li>spdlog</li><li>Microsoft GSL</li><li>Boost.DI</li><li>doctest</li><li>zlib</li>"
                                 "<li>%4</li></ul>")
                             .arg(tr("Built with"), QString::fromLatin1(qVersion()), Link("https://www.qt.io", "qt.io"),
                                  tr("Decoders written for this program, some after public descriptions: RNC, LZO, LZMA, LZ4, LZX, crunch (from Unity's "
                                     "fork, zlib license), ETC, ASTC, BC1 to BC7, Vorbis to Ogg"));
    if (!driverText.isEmpty())
    {
        components += QString("<h3>%1</h3>%2").arg(tr("Format driver"), driverText);
    }
    components += QString("<h3>%1</h3><p>%2, %3</p>").arg(tr("System"), QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
    tabs->addTab(MakePage(components, this), tr("Components"));

    tabs->addTab(MakePage(QString("<h3>Mozilla Public License 2.0</h3>"
                                  "<p>%1</p><p>%2</p><p>%3</p>")
                              .arg(tr("This program is free software. It is distributed under the terms of the Mozilla Public License, version 2.0."),
                                   tr("It is provided as it is, without any warranty. The files and the games that you open with it belong to their owners."),
                                   Link("https://www.mozilla.org/MPL/2.0/")),
                          this),
                 tr("License"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setSpacing(8);
    layout->addWidget(banner);
    auto *body = new QVBoxLayout;
    body->setContentsMargins(12, 0, 12, 0);
    body->addWidget(tabs, 1);
    body->addWidget(buttons);
    layout->addLayout(body, 1);
}
} // namespace Dup::Ui
