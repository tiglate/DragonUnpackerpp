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
