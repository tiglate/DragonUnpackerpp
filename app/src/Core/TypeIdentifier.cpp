// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/TypeIdentifier.h"

#include <algorithm>

namespace Dup::Core
{
void ArchiveTypes::Set(std::uint32_t entry, const DetectedType &type)
{
    const std::scoped_lock lock(_mutex);
    if (entry >= _perEntry.size())
    {
        return;
    }
    const std::string key = type.Id + "|" + type.Description;
    auto found = _byKey.find(key);
    if (found == _byKey.end())
    {
        found = _byKey.emplace(key, static_cast<std::uint32_t>(_types.size())).first;
        _types.push_back(type);
    }
    if (_perEntry[entry] == NONE)
    {
        ++_done;
    }
    _perEntry[entry] = found->second;
    _confidence[entry] = static_cast<std::uint8_t>(std::clamp(type.Confidence, 0, 100));
}

auto ArchiveTypes::Get(std::uint32_t entry) const -> std::optional<DetectedType>
{
    const std::scoped_lock lock(_mutex);
    if (entry >= _perEntry.size() || _perEntry[entry] == NONE)
    {
        return std::nullopt;
    }
    DetectedType type = _types[_perEntry[entry]];
    type.Confidence = _confidence[entry];
    return type;
}

auto ArchiveTypes::GetSummary() const -> std::vector<std::pair<DetectedType, std::uint32_t>>
{
    const std::scoped_lock lock(_mutex);
    std::vector<std::uint32_t> counts(_types.size(), 0);
    for (const auto type : _perEntry)
    {
        if (type != NONE)
        {
            ++counts[type];
        }
    }
    std::vector<std::pair<DetectedType, std::uint32_t>> result;
    for (std::size_t i = 0; i < _types.size(); ++i)
    {
        result.emplace_back(_types[i], counts[i]);
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    return result;
}

auto IdentifyEntry(const Dudi7::IArchive &archive, std::int64_t number, std::string_view fileName, std::int64_t size) -> DetectedType
{
    std::vector<std::byte> head;
    head.reserve(FILE_TYPE_HEAD_BYTES);
    // a sink that says no stops the extraction as soon as there is enough
    archive.Extract(number, [&](std::span<const std::byte> piece) {
        const std::size_t room = FILE_TYPE_HEAD_BYTES - head.size();
        head.insert(head.end(), piece.begin(), piece.begin() + static_cast<std::ptrdiff_t>(std::min(room, piece.size())));
        return head.size() < FILE_TYPE_HEAD_BYTES;
    });
    // when the whole entry was read its real length is known (the listed size of an entry that the driver converts differs)
    return IdentifyFile(head, head.size() < FILE_TYPE_HEAD_BYTES ? static_cast<std::int64_t>(head.size()) : size, fileName);
}

TypeIdentifier::TypeIdentifier(std::shared_ptr<const ArchiveSource> source, std::shared_ptr<const ArchiveIndex> index, std::vector<std::uint32_t> entries)
    : _source(std::move(source)), _index(std::move(index)), _entries(std::move(entries)), _types(std::make_shared<ArchiveTypes>(_index->GetEntryCount()))
{
}

TypeIdentifier::~TypeIdentifier()
{
    Cancel();
}

void TypeIdentifier::Start()
{
    _thread = std::jthread([this](const std::stop_token &stop) {
        try
        {
            const auto archive = _source->Open();
            for (const auto entry : _entries)
            {
                if (stop.stop_requested())
                {
                    break;
                }
                try
                {
                    _types->Set(entry, IdentifyEntry(*archive, entry, _index->GetFileName(entry), _index->GetSize(entry)));
                }
                catch (const std::exception &)
                {
                    // an entry that cannot be read keeps its place: the name is the only thing known
                    _types->Set(entry, DetectedType{"error", "Unreadable entry", "", FileCategory::UNKNOWN, 0});
                }
            }
        }
        catch (const std::exception &error)
        {
            const std::scoped_lock lock(_mutex);
            _error = error.what();
        }
        _done = true;
    });
}

void TypeIdentifier::Cancel()
{
    _thread.request_stop();
}

auto TypeIdentifier::GetError() const -> std::string
{
    const std::scoped_lock lock(_mutex);
    return _error;
}
} // namespace Dup::Core
