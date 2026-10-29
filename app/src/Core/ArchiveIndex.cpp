// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ArchiveIndex.h"

#include <spdlog/spdlog.h>

#include <functional>
#include <unordered_map>

namespace Dup::Core
{
namespace
{
/// A folder is identified by its parent and its own name (a/a and a are different folders).
struct FolderKey
{
    std::uint32_t Parent;
    std::string_view Name;
    auto operator==(const FolderKey &other) const -> bool = default;
};

struct FolderKeyHash
{
    auto operator()(const FolderKey &key) const -> std::size_t
    {
        return std::hash<std::string_view>()(key.Name) * 31U + key.Parent;
    }
};
} // namespace

void ArchiveIndex::AddEntry(std::string_view name, std::int64_t size, std::int64_t offset)
{
    const auto start = static_cast<std::uint32_t>(_pool.size());
    _nameStart.push_back(start);
    _pool += name;
    std::size_t last = 0;
    for (std::size_t k = 0; k < name.size(); ++k)
    {
        const bool isSeparator = _dirSeparator == "/"    ? name[k] == '/'
                                 : _dirSeparator == "\\" ? (name[k] == '\\' || name[k] == '/')
                                                         : false;
        if (isSeparator)
        {
            last = k + 1;
        }
    }
    _fileStart.push_back(start + static_cast<std::uint32_t>(last));
    _sizes.push_back(size);
    _offsets.push_back(offset);
}

auto ArchiveIndex::Build(const Dudi7::IArchive &archive) -> ArchiveIndex
{
    ArchiveIndex index;
    index._dirSeparator = archive.GetDirSeparator();
    const auto count = static_cast<std::uint32_t>(archive.GetEntryCount());
    index._nameStart.reserve(count + 1);
    index._fileStart.reserve(count);
    index._sizes.reserve(count);
    index._offsets.reserve(count);
    index._pool.reserve(static_cast<std::size_t>(count) * 24);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const auto entry = archive.GetEntry(i);
        index.AddEntry(entry.Name, entry.Size, entry.Offset);
    }
    index.BuildFolders();
    return index;
}

auto ArchiveIndex::FromNames(const std::vector<std::string> &names, std::string_view dirSeparator) -> ArchiveIndex
{
    ArchiveIndex index;
    index._dirSeparator = dirSeparator;
    for (const auto &name : names)
    {
        index.AddEntry(name, 0, 0);
    }
    index.BuildFolders();
    return index;
}

void ArchiveIndex::BuildFolders()
{
    const auto count = static_cast<std::uint32_t>(_sizes.size());
    _nameStart.push_back(static_cast<std::uint32_t>(_pool.size()));
    _entryFolder.assign(count, ROOT);

    auto isSeparator = [&](char c) {
        return _dirSeparator == "/" ? c == '/' : (_dirSeparator == "\\" ? (c == '\\' || c == '/') : false);
    };

    _folders.clear();
    _folders.push_back(Folder{});
    // the keys point into the pool, which does not change any more
    std::unordered_map<FolderKey, std::uint32_t, FolderKeyHash> byKey;
    byKey.reserve(1024);
    std::string_view lastDir;
    std::uint32_t lastFolder = ROOT;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const std::string_view path = GetPath(i);
        const std::string_view name = GetFileName(i);
        std::string_view dir = path.substr(0, path.size() - name.size());
        std::uint32_t folder = ROOT;
        if (!dir.empty())
        {
            if (dir == lastDir)
            {
                folder = lastFolder;
            }
            else
            {
                // one folder per non empty component of the directory part
                std::uint32_t parent = ROOT;
                std::size_t start = 0;
                while (start < dir.size())
                {
                    std::size_t end = start;
                    while (end < dir.size() && !isSeparator(dir[end]))
                    {
                        ++end;
                    }
                    if (end > start)
                    {
                        const FolderKey key{parent, dir.substr(start, end - start)};
                        auto found = byKey.find(key);
                        if (found == byKey.end())
                        {
                            const auto created = static_cast<std::uint32_t>(_folders.size());
                            _folders.push_back(Folder{std::string(key.Name), parent, {}, {}, 0});
                            _folders[parent].Children.push_back(created);
                            found = byKey.emplace(key, created).first;
                        }
                        parent = found->second;
                    }
                    start = end + 1;
                }
                folder = parent;
                lastDir = dir;
                lastFolder = folder;
            }
        }
        _entryFolder[i] = folder;
        _folders[folder].Entries.push_back(i);
    }

    // file counts: a child always has a larger index than its parent, so one backwards pass is enough
    for (auto &folder : _folders)
    {
        folder.FileCount = static_cast<std::uint32_t>(folder.Entries.size());
    }
    for (std::size_t f = _folders.size(); f-- > 1;)
    {
        _folders[_folders[f].Parent].FileCount += _folders[f].FileCount;
    }
    spdlog::debug("index: {} entries, {} folders", count, _folders.size());
}

auto ArchiveIndex::GetPath(std::uint32_t entry) const -> std::string_view
{
    return std::string_view(_pool).substr(_nameStart[entry], _nameStart[entry + 1] - _nameStart[entry]);
}

auto ArchiveIndex::GetFileName(std::uint32_t entry) const -> std::string_view
{
    return std::string_view(_pool).substr(_fileStart[entry], _nameStart[entry + 1] - _fileStart[entry]);
}

auto ArchiveIndex::GetEntriesRecursive(std::uint32_t folder) const -> std::vector<std::uint32_t>
{
    std::vector<std::uint32_t> result;
    std::vector<std::uint32_t> pending{folder};
    while (!pending.empty())
    {
        const auto current = pending.back();
        pending.pop_back();
        result.insert(result.end(), _folders[current].Entries.begin(), _folders[current].Entries.end());
        for (auto it = _folders[current].Children.rbegin(); it != _folders[current].Children.rend(); ++it)
        {
            pending.push_back(*it);
        }
    }
    return result;
}
} // namespace Dup::Core
