// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ArchiveIndex.h"
#include "Core/TypeIdentifier.h"

#include <QAbstractTableModel>

#include <memory>
#include <QHash>
#include <QUrl>
#include <QPixmap>

#include <functional>
#include <unordered_set>

#include <cstdint>
#include <vector>

namespace Dup::Ui
{
/// The entries of one folder (or of a folder and everything below it) as a table; sorting is done here, on indexes,
/// so that a folder of hundreds of thousands of entries sorts in a fraction of a second.
class ArchiveTableModel : public QAbstractTableModel
{
    Q_OBJECT

  public:
    enum Column
    {
        NAME,
        TYPE,
        SIZE,
        OFFSET,
        FOLDER,
        DETECTED, // what the content says (after Tools > Identify the file types)
        COLUMN_COUNT
    };

    explicit ArchiveTableModel(QObject *parent = nullptr);

    void SetIndex(const Core::ArchiveIndex *index);
    /// The types found by the identification (nullptr: none); the column shows them as they come.
    void SetTypes(std::shared_ptr<const Core::ArchiveTypes> types);
    void RefreshTypes();
    /// Shows the entries of the folder; recursive adds those of its subfolders.
    void ShowFolder(std::uint32_t folder, bool recursive);
    /// Keeps only the entries whose file name contains the text (ASCII case insensitive); empty = all.
    void SetFilter(const QString &text);
    /// Keeps only the entries of one type (the extension, as the Type column shows it; ASCII case insensitive). An empty
    /// extension selects the entries that have none. clear (SetTypeFilter(false, {})) shows every type.
    void SetTypeFilter(bool active, const QString &extension);
    /// Every type (extension, lower case) of the whole archive with the number of entries, sorted by name.
    [[nodiscard]] auto TypeCounts() const -> std::vector<std::pair<QString, std::uint32_t>>;
    /// Thumbnails (the decoration of the name column): asked through the requester, given back with SetThumbnail.
    void SetThumbnailRequester(std::function<void(std::uint32_t)> requester);
    void SetThumbnailsEnabled(bool enabled);
    /// Size of the cells of the thumbnail view (the same for every item: the view must not size them from the first one).
    void SetItemSize(const QSize &size);
    /// Colour of the small file type icons of the details view (follows the theme).
    void SetIconColour(const QColor &colour);
    void SetThumbnail(std::uint32_t entry, const QPixmap &pixmap);
    /// Forgets the thumbnails and the pending requests (a new archive).
    void ClearThumbnails();
    /// Forgets the requests of the entries that have no thumbnail yet (the visible rows changed).
    void ForgetPendingThumbnails();
    /// Drag and drop out of the application: the provider extracts the entries (numbers in the index) and gives the files.
    void SetDragProvider(std::function<QList<QUrl>(const std::vector<std::uint32_t> &)> provider);
    auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
    auto mimeTypes() const -> QStringList override;
    auto mimeData(const QModelIndexList &indexes) const -> QMimeData * override;
    [[nodiscard]] auto EntryAt(int row) const -> std::uint32_t
    {
        return _rows[static_cast<std::size_t>(row)];
    }

    auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
    auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
    auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
    auto headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const -> QVariant override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    [[nodiscard]] static auto FormatSize(std::int64_t size) -> QString;

  private:
    const Core::ArchiveIndex *_index{nullptr};
    std::vector<std::uint32_t> _source; // the entries of the shown folder
    std::vector<std::uint32_t> _rows;   // the ones that pass the filter
    std::string _filter;                // lower case
    bool _typeFilterActive{false};
    std::string _typeFilter;            // lower case extension
    void ApplyFilter();
    void RebuildRowMap();
    std::function<void(std::uint32_t)> _thumbnailRequester;
    bool _thumbnailsEnabled{false};
    std::function<QList<QUrl>(const std::vector<std::uint32_t> &)> _dragProvider;
    QSize _itemSize{124, 142};
    QColor _iconColour{0xd8, 0xd8, 0xd8};
    mutable QHash<QString, QIcon> _kindIcons;
    QHash<std::uint32_t, QPixmap> _thumbnails;
    mutable std::unordered_set<std::uint32_t> _requested; // filled by data(), which only queues work
    std::vector<std::int32_t> _rowOfEntry;
    int _sortColumn{-1};
    std::shared_ptr<const Core::ArchiveTypes> _types;
    Qt::SortOrder _sortOrder{Qt::AscendingOrder};
};
} // namespace Dup::Ui
