// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/ArchiveTableModel.h"

#include "Core/ThumbnailWorker.h"
#include "Ui/Icons.h"

#include <QMimeData>

#include <algorithm>
#include <cctype>

namespace Dup::Ui
{
namespace
{
/// ASCII case-insensitive comparison with numbers compared by value (file2 < file10).
auto NaturalLess(std::string_view a, std::string_view b) -> bool
{
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() && j < b.size())
    {
        const auto ca = static_cast<unsigned char>(a[i]);
        const auto cb = static_cast<unsigned char>(b[j]);
        if (std::isdigit(ca) != 0 && std::isdigit(cb) != 0)
        {
            std::size_t ei = i;
            std::size_t ej = j;
            while (ei < a.size() && std::isdigit(static_cast<unsigned char>(a[ei])) != 0)
            {
                ++ei;
            }
            while (ej < b.size() && std::isdigit(static_cast<unsigned char>(b[ej])) != 0)
            {
                ++ej;
            }
            std::size_t si = i;
            std::size_t sj = j;
            while (si + 1 < ei && a[si] == '0')
            {
                ++si;
            }
            while (sj + 1 < ej && b[sj] == '0')
            {
                ++sj;
            }
            if (ei - si != ej - sj)
            {
                return ei - si < ej - sj;
            }
            const int cmp = a.substr(si, ei - si).compare(b.substr(sj, ej - sj));
            if (cmp != 0)
            {
                return cmp < 0;
            }
            i = ei;
            j = ej;
            continue;
        }
        const int la = std::tolower(ca);
        const int lb = std::tolower(cb);
        if (la != lb)
        {
            return la < lb;
        }
        ++i;
        ++j;
    }
    return (a.size() - i) < (b.size() - j);
}

auto ExtensionOf(std::string_view name) -> std::string_view
{
    const auto dot = name.rfind('.');
    return dot == std::string_view::npos ? std::string_view() : name.substr(dot + 1);
}
} // namespace

ArchiveTableModel::ArchiveTableModel(QObject *parent) : QAbstractTableModel(parent)
{
}

void ArchiveTableModel::SetIndex(const Core::ArchiveIndex *index)
{
    beginResetModel();
    _index = index;
    _source.clear();
    _rows.clear();
    _rowOfEntry.clear();
    ClearThumbnails();
    endResetModel();
}

void ArchiveTableModel::ShowFolder(std::uint32_t folder, bool recursive)
{
    beginResetModel();
    _source.clear();
    if (_index != nullptr)
    {
        _source = recursive ? _index->GetEntriesRecursive(folder) : _index->GetFolders()[folder].Entries;
    }
    ApplyFilter();
    RebuildRowMap();
    endResetModel();
    if (_sortColumn >= 0)
    {
        sort(_sortColumn, _sortOrder);
    }
}

void ArchiveTableModel::SetFilter(const QString &text)
{
    beginResetModel();
    _filter = text.toUtf8().toLower().toStdString();
    ApplyFilter();
    RebuildRowMap();
    endResetModel();
    if (_sortColumn >= 0)
    {
        sort(_sortColumn, _sortOrder);
    }
}

void ArchiveTableModel::ApplyFilter()
{
    _rows.clear();
    if (_filter.empty() || _index == nullptr)
    {
        _rows = _source;
        return;
    }
    for (const auto entry : _source)
    {
        std::string name(_index->GetFileName(entry));
        for (auto &c : name)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (name.find(_filter) != std::string::npos)
        {
            _rows.push_back(entry);
        }
    }
}

auto ArchiveTableModel::rowCount(const QModelIndex &parent) const -> int
{
    return parent.isValid() ? 0 : static_cast<int>(_rows.size());
}

auto ArchiveTableModel::columnCount(const QModelIndex &parent) const -> int
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

auto ArchiveTableModel::FormatSize(std::int64_t size) -> QString
{
    if (size < 1024)
    {
        return QString::number(size) + " B";
    }
    double value = static_cast<double>(size);
    const char *units[] = {"KB", "MB", "GB", "TB"};
    int unit = -1;
    while (value >= 1024.0 && unit < 3)
    {
        value /= 1024.0;
        ++unit;
    }
    return QString::number(value, 'f', value < 10 ? 2 : 1) + " " + units[unit];
}

auto ArchiveTableModel::data(const QModelIndex &index, int role) const -> QVariant
{
    if (_index == nullptr || !index.isValid())
    {
        return {};
    }
    const auto entry = _rows[static_cast<std::size_t>(index.row())];
    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case NAME: {
            const auto name = _index->GetFileName(entry);
            return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
        }
        case TYPE: {
            const auto extension = ExtensionOf(_index->GetFileName(entry));
            return QString::fromUtf8(extension.data(), static_cast<qsizetype>(extension.size())).toUpper();
        }
        case DETECTED: {
            if (_types != nullptr)
            {
                if (const auto type = _types->Get(entry))
                {
                    return QString::fromStdString(type->Description);
                }
            }
            return {};
        }
        case SIZE:
            return FormatSize(_index->GetSize(entry));
        case OFFSET:
            return QString::number(_index->GetOffset(entry));
        case FOLDER: {
            const auto path = _index->GetPath(entry);
            const auto name = _index->GetFileName(entry);
            auto folder = path.substr(0, path.size() - name.size());
            while (!folder.empty() && (folder.back() == '/' || folder.back() == '\\'))
            {
                folder.remove_suffix(1);
            }
            return QString::fromUtf8(folder.data(), static_cast<qsizetype>(folder.size()));
        }
        default:
            break;
        }
    }
    else if (role == Qt::SizeHintRole && _thumbnailsEnabled)
    {
        return _itemSize;
    }
    else if (role == Qt::DecorationRole && index.column() == NAME && !_thumbnailsEnabled)
    {
        // the small icon of the kind of file (by extension), made once per kind
        const auto extension = QString::fromUtf8(ExtensionOf(_index->GetFileName(entry)).data(), static_cast<qsizetype>(ExtensionOf(_index->GetFileName(entry)).size())).toLower();
        auto cached = _kindIcons.find(extension);
        if (cached == _kindIcons.end())
        {
            cached = _kindIcons.insert(extension, Icons::ForExtension(extension, _iconColour));
        }
        return *cached;
    }
    else if (role == Qt::DecorationRole && index.column() == NAME && _thumbnailsEnabled)
    {
        const auto cached = _thumbnails.constFind(entry);
        if (cached != _thumbnails.constEnd())
        {
            return *cached;
        }
        if (_thumbnailRequester && Core::HasThumbnail(_index->GetFileName(entry)) && _requested.insert(entry).second)
        {
            _thumbnailRequester(entry); // the model is const here: the requester only queues work
        }
        return {};
    }
    else if (role == Qt::TextAlignmentRole && (index.column() == SIZE || index.column() == OFFSET))
    {
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }
    return {};
}

auto ArchiveTableModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }
    switch (section)
    {
    case NAME:
        return tr("Name");
    case TYPE:
        return tr("Type");
    case SIZE:
        return tr("Size");
    case OFFSET:
        return tr("Offset");
    case FOLDER:
        return tr("Folder");
    case DETECTED:
        return tr("Detected type");
    default:
        return {};
    }
}

void ArchiveTableModel::sort(int column, Qt::SortOrder order)
{
    if (_index == nullptr || column < 0 || column >= COLUMN_COUNT)
    {
        return;
    }
    _sortColumn = column;
    _sortOrder = order;
    emit layoutAboutToBeChanged();
    const bool descending = order == Qt::DescendingOrder;
    const Core::ArchiveIndex &index = *_index;
    auto less = [&](std::uint32_t a, std::uint32_t b) {
        switch (column)
        {
        case NAME:
            return NaturalLess(index.GetFileName(a), index.GetFileName(b));
        case TYPE:
            return NaturalLess(ExtensionOf(index.GetFileName(a)), ExtensionOf(index.GetFileName(b)));
        case FOLDER:
            return NaturalLess(index.GetPath(a), index.GetPath(b));
        case DETECTED: {
            const auto describe = [&](std::uint32_t entry) {
                const auto type = _types != nullptr ? _types->Get(entry) : std::nullopt;
                return type ? type->Description : std::string();
            };
            return describe(a) < describe(b);
        }
        case SIZE:
            return index.GetSize(a) < index.GetSize(b);
        default:
            return index.GetOffset(a) < index.GetOffset(b);
        }
    };
    if (descending)
    {
        std::stable_sort(_rows.begin(), _rows.end(), [&](std::uint32_t a, std::uint32_t b) { return less(b, a); });
    }
    else
    {
        std::stable_sort(_rows.begin(), _rows.end(), less);
    }
    RebuildRowMap();
    emit layoutChanged();
}
} // namespace Dup::Ui

namespace Dup::Ui
{
void ArchiveTableModel::SetTypes(std::shared_ptr<const Core::ArchiveTypes> types)
{
    _types = std::move(types);
    RefreshTypes();
}

void ArchiveTableModel::RefreshTypes()
{
    if (!_rows.empty())
    {
        emit dataChanged(index(0, DETECTED), index(rowCount() - 1, DETECTED), {Qt::DisplayRole});
    }
}

void ArchiveTableModel::SetThumbnailRequester(std::function<void(std::uint32_t)> requester)
{
    _thumbnailRequester = std::move(requester);
}

void ArchiveTableModel::SetThumbnailsEnabled(bool enabled)
{
    if (_thumbnailsEnabled == enabled)
    {
        return;
    }
    _thumbnailsEnabled = enabled;
    RebuildRowMap();
    if (!_rows.empty())
    {
        emit dataChanged(index(0, NAME), index(rowCount() - 1, NAME), {Qt::DecorationRole});
    }
}

void ArchiveTableModel::RebuildRowMap()
{
    _rowOfEntry.clear();
    if (!_thumbnailsEnabled || _index == nullptr)
    {
        return;
    }
    _rowOfEntry.assign(_index->GetEntryCount(), -1);
    for (std::size_t row = 0; row < _rows.size(); ++row)
    {
        _rowOfEntry[_rows[row]] = static_cast<std::int32_t>(row);
    }
}

void ArchiveTableModel::SetThumbnail(std::uint32_t entry, const QPixmap &pixmap)
{
    if (_thumbnails.size() > 4000)
    {
        _thumbnails.clear(); // a simple bound: the visible ones are asked for again
        _requested.clear();
    }
    _thumbnails.insert(entry, pixmap.isNull() ? QPixmap(1, 1) : pixmap);
    if (entry < _rowOfEntry.size() && _rowOfEntry[entry] >= 0)
    {
        const auto at = index(_rowOfEntry[entry], NAME);
        emit dataChanged(at, at, {Qt::DecorationRole});
    }
}

void ArchiveTableModel::ClearThumbnails()
{
    _thumbnails.clear();
    _requested.clear();
}

void ArchiveTableModel::ForgetPendingThumbnails()
{
    for (auto it = _requested.begin(); it != _requested.end();)
    {
        it = _thumbnails.contains(*it) ? std::next(it) : _requested.erase(it);
    }
}
} // namespace Dup::Ui

namespace Dup::Ui
{
void ArchiveTableModel::SetDragProvider(std::function<QList<QUrl>(const std::vector<std::uint32_t> &)> provider)
{
    _dragProvider = std::move(provider);
}

auto ArchiveTableModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
    auto result = QAbstractTableModel::flags(index);
    if (index.isValid() && _dragProvider)
    {
        result |= Qt::ItemIsDragEnabled;
    }
    return result;
}

auto ArchiveTableModel::mimeTypes() const -> QStringList
{
    return {"text/uri-list"};
}

auto ArchiveTableModel::mimeData(const QModelIndexList &indexes) const -> QMimeData *
{
    if (!_dragProvider)
    {
        return nullptr;
    }
    std::vector<std::uint32_t> entries;
    for (const auto &index : indexes)
    {
        if (index.column() == NAME)
        {
            entries.push_back(_rows[static_cast<std::size_t>(index.row())]);
        }
    }
    const QList<QUrl> urls = _dragProvider(entries);
    if (urls.isEmpty())
    {
        return nullptr;
    }
    auto *data = new QMimeData;
    data->setUrls(urls);
    return data;
}

void ArchiveTableModel::SetItemSize(const QSize &size)
{
    _itemSize = size;
}

void ArchiveTableModel::SetIconColour(const QColor &colour)
{
    _iconColour = colour;
    _kindIcons.clear();
    if (!_rows.empty())
    {
        emit dataChanged(index(0, NAME), index(rowCount() - 1, NAME), {Qt::DecorationRole});
    }
}
} // namespace Dup::Ui
