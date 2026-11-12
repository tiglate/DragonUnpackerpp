// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/Icons.h"

#include <QByteArray>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

#include <set>

namespace Dup::Ui
{
namespace
{
/// Inner SVG elements: stroke only, round joins, "currentColor" is replaced by the colour asked for.
const QHash<QString, QString> &Shapes()
{
    static const QHash<QString, QString> shapes{
        {"open", R"(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>)"},
        {"extract", R"(<path d="M12 3v11m0 0l-4-4m4 4l4-4M4 20h16"/>)"},
        {"extract-all", R"(<path d="M12 3v9m0 0l-3.5-3.5M12 12l3.5-3.5M4 16h16M4 20h16"/>)"},
        {"details", R"(<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>)"},
        {"thumbnails", R"(<rect x="4" y="4" width="7" height="7" rx="1"/><rect x="13" y="4" width="7" height="7" rx="1"/><rect x="4" y="13" width="7" height="7" rx="1"/><rect x="13" y="13" width="7" height="7" rx="1"/>)"},
        {"back", R"(<path d="M15 5l-7 7 7 7"/>)"},
        {"forward", R"(<path d="M9 5l7 7-7 7"/>)"},
        {"up", R"(<path d="M5 15l7-7 7 7"/>)"},
        {"search", R"(<circle cx="11" cy="11" r="6"/><path d="M16 16l5 5"/>)"},
        {"hyperripper", R"(<circle cx="10" cy="10" r="6"/><path d="M15 15l6 6M8 10h4M10 8v4"/>)"},
        {"options", R"(<path d="M4 7h10M18 7h2M4 17h2M10 17h10"/><circle cx="16" cy="7" r="2"/><circle cx="8" cy="17" r="2"/>)"},
        {"file", R"(<path d="M6 3h8l5 5v13H6z"/><path d="M14 3v5h5"/>)"},
        {"image", R"(<rect x="3" y="5" width="18" height="14" rx="2"/><circle cx="9" cy="10" r="1.5"/><path d="M4 18l5-5 4 4 3-3 4 4"/>)"},
        {"sound", R"(<path d="M4 9v6h4l5 4V5L8 9z"/><path d="M16 9a4 4 0 0 1 0 6M18.5 6.5a8 8 0 0 1 0 11"/>)"},
        {"text", R"(<path d="M6 3h8l5 5v13H6z"/><path d="M9 12h7M9 15h7M9 18h4"/>)"},
        {"folder", R"(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>)"},
        // the buttons of the 3D viewers
        {"turn-left", R"(<path d="M4.5 13a7.5 7.5 0 1 0 2.2-6.3"/><path d="M4 3.5v5h5"/>)"},
        {"turn-right", R"(<path d="M19.5 13a7.5 7.5 0 1 1-2.2-6.3"/><path d="M20 3.5v5h-5"/>)"},
        {"tilt-up", R"(<ellipse cx="12" cy="16" rx="8" ry="3.5"/><path d="M12 13V3M8.5 6.5L12 3l3.5 3.5"/>)"},
        {"tilt-down", R"(<ellipse cx="12" cy="8" rx="8" ry="3.5"/><path d="M12 11v10M8.5 17.5L12 21l3.5-3.5"/>)"},
        {"move-left", R"(<path d="M20 12H4M9 7l-5 5 5 5"/>)"},
        {"move-right", R"(<path d="M4 12h16M15 7l5 5-5 5"/>)"},
        {"move-up", R"(<path d="M12 20V4M7 9l5-5 5 5"/>)"},
        {"move-down", R"(<path d="M12 4v16M7 15l5 5 5-5"/>)"},
        {"zoom-in", R"(<circle cx="11" cy="11" r="6.5"/><path d="M16 16l5 5M8 11h6M11 8v6"/>)"},
        {"zoom-out", R"(<circle cx="11" cy="11" r="6.5"/><path d="M16 16l5 5M8 11h6"/>)"},
        {"frame", R"(<path d="M4 9V4h5M15 4h5v5M20 15v5h-5M9 20H4v-5"/><rect x="9" y="9" width="6" height="6" rx="1"/>)"},
        {"wireframe", R"(<path d="M12 3l8 4.5v9L12 21l-8-4.5v-9z"/><path d="M12 12l8-4.5M12 12v9M12 12L4 7.5"/>)"},
        {"backfaces", R"(<path d="M3 19L12 4l9 15z"/><path d="M7.5 15.5h9" stroke-dasharray="1.5 2.5"/>)"},
        {"lightmap", R"(<path d="M9 18h6M10 21h4"/><path d="M12 3a6 6 0 0 0-3.5 10.9V16h7v-2.1A6 6 0 0 0 12 3z"/>)"},
        {"brighter", R"(<circle cx="12" cy="12" r="4"/><path d="M12 2v2.5M12 19.5V22M2 12h2.5M19.5 12H22M4.9 4.9l1.8 1.8M17.3 17.3l1.8 1.8M4.9 19.1l1.8-1.8M17.3 6.7l1.8-1.8"/>)"},
        {"darker", R"(<path d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/>)"},
        {"help", R"(<circle cx="12" cy="12" r="9"/><path d="M9.5 9.5a2.5 2.5 0 1 1 3.6 2.2c-.7.4-1.1 1-1.1 1.8v.5M12 17.2v.1"/>)"},
    };
    return shapes;
}

auto Render(const QString &name, const QColor &colour, int size) -> QPixmap
{
    const auto shape = Shapes().constFind(name);
    if (shape == Shapes().constEnd())
    {
        return {};
    }
    const QByteArray svg = QString(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">%2</svg>)")
                               .arg(colour.name(), *shape)
                               .toUtf8();
    QSvgRenderer renderer(svg);
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    return pixmap;
}
} // namespace

auto Icons::Make(const QString &name, const QColor &colour) -> QIcon
{
    QIcon icon;
    for (const int size : {16, 20, 24, 32, 48})
    {
        const QPixmap pixmap = Render(name, colour, size);
        if (pixmap.isNull())
        {
            return {};
        }
        icon.addPixmap(pixmap);
    }
    return icon;
}

auto Icons::ForExtension(const QString &extension, const QColor &colour) -> QIcon
{
    static const std::set<QString> images{"tga", "dds", "png", "jpg", "jpeg", "bmp", "gif", "webp", "pcx", "tif", "tiff", "tex", "art", "ico"};
    static const std::set<QString> sounds{"wav", "mp3", "ogg", "flac", "fsb", "mid", "midi", "xm", "mod", "it", "s3m", "snd", "voc", "aif", "aiff"};
    static const std::set<QString> texts{"txt", "json", "xml", "ini", "cfg", "lua", "c", "h", "csv", "md", "fnt", "obj", "mtl", "log", "bat", "sh", "html", "css", "js"};
    const char *name = images.contains(extension) ? "image" : sounds.contains(extension) ? "sound" : texts.contains(extension) ? "text" : "file";
    return Make(name, colour);
}
} // namespace Dup::Ui
