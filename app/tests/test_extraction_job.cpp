// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ExtractionJob.h"
#include "TestSupport.h"

#include <chrono>
#include <numeric>

using namespace Dup;

namespace
{
auto MakeRequest(const Dudi7::Driver &driver, const std::filesystem::path &file, const std::filesystem::path &destination) -> Core::ExtractionJob::Request
{
    const auto archive = driver.Open(file);
    auto index = std::make_shared<const Core::ArchiveIndex>(Core::ArchiveIndex::Build(*archive));
    Core::ExtractionJob::Request request;
    request.Title = file.filename().string();
    request.Source = std::make_shared<Core::DriverArchiveSource>(driver, file);
    request.Index = index;
    request.Entries.resize(index->GetEntryCount());
    std::iota(request.Entries.begin(), request.Entries.end(), 0U);
    request.Destination = destination;
    return request;
}
} // namespace

TEST_CASE("An extraction job runs on its own thread and reports the result")
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    Tests::TempDirectory out;
    Core::ExtractionJob job(MakeRequest(*driver, Tests::Fixture("dups.wad"), out.GetPath()));
    CHECK(job.GetState() == Core::JobState::PENDING);
    job.Start();
    job.Wait();
    CHECK(job.GetState() == Core::JobState::FINISHED);
    CHECK(job.GetStats().Extracted == 6);
    CHECK(job.GetBytesDone() == job.GetBytesTotal());
    CHECK(Tests::CountFiles(out.GetPath()) == 6);
    CHECK(Tests::ReadFile(out.GetPath() / "THINGS~2") == "things of map 2");
}

TEST_CASE("The conflict handler runs on the job's thread, and a cancel from it stops the job")
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    Tests::TempDirectory out;
    {
        Core::ExtractionJob first(MakeRequest(*driver, Tests::Fixture("dups.wad"), out.GetPath()));
        first.Start();
        first.Wait();
    }
    const auto mainThread = std::this_thread::get_id();
    std::atomic<bool> onOtherThread{false};
    Core::ExtractionJob second(MakeRequest(*driver, Tests::Fixture("dups.wad"), out.GetPath()), [&](const std::filesystem::path &) {
        onOtherThread = std::this_thread::get_id() != mainThread;
        return Core::ConflictAction::CANCEL;
    });
    second.Start();
    second.Wait();
    CHECK(onOtherThread);
    CHECK(second.GetState() == Core::JobState::CANCELLED);
}

TEST_CASE("Cancel stops the job and leaves no partial file")
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    Tests::TempDirectory out;
    for (int round = 0; round < 20; ++round)
    {
        Core::ExtractionJob job(MakeRequest(*driver, Tests::Fixture("test.pak"), out.GetPath() / std::to_string(round)));
        job.Start();
        job.Cancel();
        job.Wait();
        const auto state = job.GetState();
        CHECK((state == Core::JobState::CANCELLED || state == Core::JobState::FINISHED));
        if (state == Core::JobState::CANCELLED)
        {
            // whatever was written is complete: sizes of the pak entries are 24 / 76800 / 0
            const auto big = out.GetPath() / std::to_string(round) / "sub" / "dir" / "data.bin";
            if (std::filesystem::exists(big))
            {
                CHECK(std::filesystem::file_size(big) == 76800);
            }
        }
    }
}

TEST_CASE("A job on an archive that cannot be opened again fails cleanly")
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    Tests::TempDirectory out;
    auto request = MakeRequest(*driver, Tests::Fixture("test.pak"), out.GetPath());
    request.Source = std::make_shared<Core::DriverArchiveSource>(*driver, out.GetPath() / "missing.pak");
    Core::ExtractionJob job(request);
    job.Start();
    job.Wait();
    CHECK(job.GetState() == Core::JobState::FAILED);
    CHECK_FALSE(job.GetError().empty());
}
