// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ExtractionJob.h"

#include <spdlog/spdlog.h>

#include <utility>

namespace Dup::Core
{
ExtractionJob::ExtractionJob(Request request,
                             std::function<ConflictAction(const std::filesystem::path &)> onConflict)
    : _request(std::move(request)), _onConflict(std::move(onConflict))
{
    std::int64_t total = 0;
    for (const auto entry : _request.Entries)
    {
        total += _request.Index->GetSize(entry);
    }
    _bytesTotal = total;
}

ExtractionJob::~ExtractionJob()
{
    Cancel();
    Wait();
}

void ExtractionJob::Start()
{
    JobState expected = JobState::PENDING;
    if (_state.compare_exchange_strong(expected, JobState::RUNNING))
    {
        _thread = std::jthread([this](const std::stop_token &stop) { Run(stop); });
    }
}

void ExtractionJob::Cancel()
{
    _thread.request_stop();
}

void ExtractionJob::Wait()
{
    if (_thread.joinable())
    {
        _thread.join();
    }
}

auto ExtractionJob::GetCurrentEntry() const -> std::string
{
    const std::scoped_lock lock(_mutex);
    return _currentEntry;
}

auto ExtractionJob::GetStats() const -> ExtractStats
{
    const std::scoped_lock lock(_mutex);
    return _stats;
}

auto ExtractionJob::GetError() const -> std::string
{
    const std::scoped_lock lock(_mutex);
    return _error;
}

void ExtractionJob::Run(const std::stop_token &stop)
{
    try
    {
        const auto archive = _request.Source->Open();
        const Extractor extractor(*archive, *_request.Index);
        ExtractCallbacks callbacks;
        callbacks.OnConflict = [this, &stop](const std::filesystem::path &path) {
            if (stop.stop_requested())
            {
                return ConflictAction::CANCEL;
            }
            return _onConflict ? _onConflict(path) : ConflictAction::OVERWRITE;
        };
        callbacks.OnProgress = [this, &stop](std::int64_t done, std::int64_t, std::string_view name) {
            _bytesDone = done;
            {
                const std::scoped_lock lock(_mutex);
                _currentEntry = name;
            }
            return !stop.stop_requested();
        };
        auto stats = extractor.Run(_request.Entries, _request.Destination, callbacks, _request.Options);
        const bool cancelled = stats.Cancelled;
        {
            const std::scoped_lock lock(_mutex);
            _stats = std::move(stats);
        }
        if (!cancelled)
        {
            _bytesDone = _bytesTotal.load();
        }
        _state = cancelled ? JobState::CANCELLED : JobState::FINISHED;
    }
    catch (const std::exception &error)
    {
        spdlog::error("extraction job \"{}\" failed: {}", _request.Title, error.what());
        {
            const std::scoped_lock lock(_mutex);
            _error = error.what();
        }
        _state = JobState::FAILED;
    }
}
} // namespace Dup::Core
