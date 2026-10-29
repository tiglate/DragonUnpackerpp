// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ArchiveIndex.h"
#include "Core/ArchiveSource.h"
#include "Core/FileTypes.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Dup::Core
{
/// What was found for the entries of an archive (shared between the thread that identifies and the one that shows).
class ArchiveTypes
{
  public:
    explicit ArchiveTypes(std::uint32_t entryCount) : _perEntry(entryCount, NONE), _confidence(entryCount, 0)
    {
    }

    void Set(std::uint32_t entry, const DetectedType &type);
    /// Nothing for an entry that was not identified (yet).
    [[nodiscard]] auto Get(std::uint32_t entry) const -> std::optional<DetectedType>;
    [[nodiscard]] auto GetDone() const -> std::uint32_t
    {
        return _done;
    }
    [[nodiscard]] auto GetCount() const -> std::uint32_t
    {
        return static_cast<std::uint32_t>(_perEntry.size());
    }
    /// How many entries have each type (by description), most numerous first.
    [[nodiscard]] auto GetSummary() const -> std::vector<std::pair<DetectedType, std::uint32_t>>;

  private:
    static constexpr std::uint32_t NONE = 0xFFFFFFFFU;
    mutable std::mutex _mutex;
    std::vector<DetectedType> _types;
    std::unordered_map<std::string, std::uint32_t> _byKey;
    std::vector<std::uint32_t> _perEntry;
    std::vector<std::uint8_t> _confidence;
    std::atomic<std::uint32_t> _done{0};
};

/// Reads the start of the entries on its own thread (with its own archive handle) and identifies them.
class TypeIdentifier
{
  public:
    TypeIdentifier(std::shared_ptr<const ArchiveSource> source, std::shared_ptr<const ArchiveIndex> index, std::vector<std::uint32_t> entries);
    ~TypeIdentifier();
    TypeIdentifier(const TypeIdentifier &) = delete;
    auto operator=(const TypeIdentifier &) -> TypeIdentifier & = delete;

    void Start();
    void Cancel();
    [[nodiscard]] auto IsDone() const -> bool
    {
        return _done;
    }
    [[nodiscard]] auto GetTypes() const -> std::shared_ptr<ArchiveTypes>
    {
        return _types;
    }
    [[nodiscard]] auto GetTotal() const -> std::uint32_t
    {
        return static_cast<std::uint32_t>(_entries.size());
    }
    [[nodiscard]] auto GetError() const -> std::string;

  private:
    std::shared_ptr<const ArchiveSource> _source;
    std::shared_ptr<const ArchiveIndex> _index;
    std::vector<std::uint32_t> _entries;
    std::shared_ptr<ArchiveTypes> _types;
    std::atomic<bool> _done{false};
    mutable std::mutex _mutex;
    std::string _error;
    std::jthread _thread;
};

/// What the first bytes of an entry say about it (the entry is read from its start up to FILE_TYPE_HEAD_BYTES; a name is only
/// used when the content tells nothing).
[[nodiscard]] auto IdentifyEntry(const Dudi7::IArchive &archive, std::int64_t number, std::string_view fileName, std::int64_t size) -> DetectedType;
} // namespace Dup::Core
