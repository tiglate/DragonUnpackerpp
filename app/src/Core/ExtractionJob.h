// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ArchiveIndex.h"
#include "Core/ArchiveSource.h"
#include "Core/Extractor.h"
#include "Dudi7/Driver.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Dup::Core
{
enum class JobState
{
    PENDING,
    RUNNING,
    FINISHED, // ran to the end (some entries may have failed or been skipped, see the statistics)
    CANCELLED,
    FAILED // the archive could not be opened again, nothing was extracted
};

/// One extraction on its own thread. The job opens its own handle on the archive (one handle per thread, see the thread
/// safety rule of the driver), so the window can go on browsing and previewing meanwhile. The state can be polled from any
/// thread. No Qt here.
class ExtractionJob
{
  public:
    struct Request
    {
        std::string Title;
        std::shared_ptr<const ArchiveSource> Source;
        std::shared_ptr<const ArchiveIndex> Index; // the index of that archive
        std::vector<std::uint32_t> Entries;
        std::filesystem::path Destination;
        ExtractOptions Options;
    };

    /// onConflict is called on the job's thread (it may block, for instance to ask the user); empty = overwrite.
    ExtractionJob(Request request,
                  std::function<ConflictAction(const std::filesystem::path &)> onConflict = {});
    ~ExtractionJob(); // cancels and waits

    ExtractionJob(const ExtractionJob &) = delete;
    auto operator=(const ExtractionJob &) -> ExtractionJob & = delete;

    void Start();
    void Cancel();
    /// Waits for the end of the thread.
    void Wait();

    [[nodiscard]] auto GetState() const -> JobState
    {
        return _state;
    }
    [[nodiscard]] auto IsDone() const -> bool
    {
        const auto state = _state.load();
        return state == JobState::FINISHED || state == JobState::CANCELLED || state == JobState::FAILED;
    }
    [[nodiscard]] auto GetBytesDone() const -> std::int64_t
    {
        return _bytesDone;
    }
    [[nodiscard]] auto GetBytesTotal() const -> std::int64_t
    {
        return _bytesTotal;
    }
    [[nodiscard]] auto GetCurrentEntry() const -> std::string;
    [[nodiscard]] auto GetRequest() const -> const Request &
    {
        return _request;
    }
    /// Valid once IsDone().
    [[nodiscard]] auto GetStats() const -> ExtractStats;
    [[nodiscard]] auto GetError() const -> std::string;

  private:
    void Run(const std::stop_token &stop);

    Request _request;
    std::function<ConflictAction(const std::filesystem::path &)> _onConflict;
    std::atomic<JobState> _state{JobState::PENDING};
    std::atomic<std::int64_t> _bytesDone{0};
    std::atomic<std::int64_t> _bytesTotal{0};
    mutable std::mutex _mutex; // protects the three members below
    std::string _currentEntry;
    ExtractStats _stats;
    std::string _error;
    std::jthread _thread;
};
} // namespace Dup::Core
