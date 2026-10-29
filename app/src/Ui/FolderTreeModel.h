// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ArchiveIndex.h"

#include <QAbstractItemModel>

namespace Dup::Ui
{
/// The folder tree of an archive (the "Hierarchy" panel); the internal id of an index is the folder number.
class FolderTreeModel : public QAbstractItemModel
{
    Q_OBJECT

  public:
    explicit FolderTreeModel(QObject *parent = nullptr);

    void SetIndex(const Core::ArchiveIndex *index, const QString &rootName);
    [[nodiscard]] static auto FolderOf(const QModelIndex &index) -> std::uint32_t
    {
        return index.isValid() ? static_cast<std::uint32_t>(index.internalId()) : Core::ArchiveIndex::ROOT;
    }

    auto index(int row, int column, const QModelIndex &parent = QModelIndex()) const -> QModelIndex override;
    auto parent(const QModelIndex &child) const -> QModelIndex override;
    auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
    auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
    auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;

  private:
    const Core::ArchiveIndex *_index{nullptr};
    QString _rootName;
};
} // namespace Dup::Ui
