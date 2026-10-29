// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/FolderTreeModel.h"

#include <algorithm>

namespace Dup::Ui
{
FolderTreeModel::FolderTreeModel(QObject *parent) : QAbstractItemModel(parent)
{
}

void FolderTreeModel::SetIndex(const Core::ArchiveIndex *index, const QString &rootName)
{
    beginResetModel();
    _index = index;
    _rootName = rootName;
    endResetModel();
}

// The root folder (the archive) is the only top level row; its internal id is 0.
auto FolderTreeModel::index(int row, int column, const QModelIndex &parent) const -> QModelIndex
{
    if (_index == nullptr || column != 0 || row < 0)
    {
        return {};
    }
    if (!parent.isValid())
    {
        return row == 0 ? createIndex(0, 0, static_cast<quintptr>(Core::ArchiveIndex::ROOT)) : QModelIndex();
    }
    const auto &children = _index->GetFolders()[FolderOf(parent)].Children;
    if (static_cast<std::size_t>(row) >= children.size())
    {
        return {};
    }
    return createIndex(row, 0, static_cast<quintptr>(children[static_cast<std::size_t>(row)]));
}

auto FolderTreeModel::parent(const QModelIndex &child) const -> QModelIndex
{
    if (_index == nullptr || !child.isValid() || FolderOf(child) == Core::ArchiveIndex::ROOT)
    {
        return {};
    }
    const auto &folders = _index->GetFolders();
    const auto parentFolder = folders[FolderOf(child)].Parent;
    if (parentFolder == Core::ArchiveIndex::ROOT)
    {
        return createIndex(0, 0, static_cast<quintptr>(Core::ArchiveIndex::ROOT));
    }
    const auto &siblings = folders[folders[parentFolder].Parent].Children;
    const auto position = std::find(siblings.begin(), siblings.end(), parentFolder) - siblings.begin();
    return createIndex(static_cast<int>(position), 0, static_cast<quintptr>(parentFolder));
}

auto FolderTreeModel::rowCount(const QModelIndex &parent) const -> int
{
    if (_index == nullptr)
    {
        return 0;
    }
    if (!parent.isValid())
    {
        return 1;
    }
    return static_cast<int>(_index->GetFolders()[FolderOf(parent)].Children.size());
}

auto FolderTreeModel::columnCount(const QModelIndex &) const -> int
{
    return 1;
}

auto FolderTreeModel::data(const QModelIndex &index, int role) const -> QVariant
{
    if (_index == nullptr || !index.isValid() || role != Qt::DisplayRole)
    {
        return {};
    }
    const auto folder = FolderOf(index);
    if (folder == Core::ArchiveIndex::ROOT)
    {
        return _rootName;
    }
    return QString::fromStdString(_index->GetFolders()[folder].Name);
}
} // namespace Dup::Ui
