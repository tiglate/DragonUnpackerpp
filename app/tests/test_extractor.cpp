// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ArchiveIndex.h"
#include "Core/Extractor.h"
#include "Dudi7/Driver.h"
#include "TestSupport.h"

#include <numeric>

using namespace Dup;

namespace
{
auto AllEntries(const Core::ArchiveIndex &index) -> std::vector<std::uint32_t>
{
    std::vector<std::uint32_t> all(index.GetEntryCount());
    std::iota(all.begin(), all.end(), 0U);
    return all;
}
} // namespace

TEST_CASE("Duplicated lump names are extracted under unique names, conflicts are reported")
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    const auto archive = driver->Open(Tests::Fixture("dups.wad"));
    const auto index = Core::ArchiveIndex::Build(*archive);
    const Core::Extractor extractor(*archive, index);
    Tests::TempDirectory out;
    const auto all = AllEntries(index);

    auto stats = extractor.Run(all, out.GetPath());
    CHECK(stats.Extracted == 6);
    CHECK(stats.Failed == 0);
    CHECK_FALSE(stats.Cancelled);
    CHECK(Tests::CountFiles(out.GetPath()) == 6);
    CHECK(Tests::ReadFile(out.GetPath() / "THINGS") == "things of map 1");
    CHECK(Tests::ReadFile(out.GetPath() / "THINGS~2") == "things of map 2");
    CHECK(Tests::ReadFile(out.GetPath() / "LINEDEFS~2") == "lines of map 2");

    // a second run finds every file: the handler says skip
    int conflicts = 0;
    Core::ExtractCallbacks callbacks;
    callbacks.OnConflict = [&](const std::filesystem::path &) {
        ++conflicts;
        return Core::ConflictAction::SKIP;
    };
    stats = extractor.Run(all, out.GetPath(), callbacks);
    CHECK(stats.Skipped == 6);
    CHECK(stats.Extracted == 0);
    CHECK(conflicts == 6);

    // "overwrite all" asks once
    conflicts = 0;
    callbacks.OnConflict = [&](const std::filesystem::path &) {
        ++conflicts;
        return Core::ConflictAction::OVERWRITE_ALL;
    };
    stats = extractor.Run(all, out.GetPath(), callbacks);
    CHECK(stats.Extracted == 6);
    CHECK(conflicts == 1);

    // cancel from the conflict handler
    callbacks.OnConflict = [](const std::filesystem::path &) { return Core::ConflictAction::CANCEL; };
    stats = extractor.Run(all, out.GetPath(), callbacks);
    CHECK(stats.Cancelled);
}

TEST_CASE("Folders are created and a cancel leaves no partial file")
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    const auto archive = driver->Open(Tests::Fixture("test.pak"));
    const auto index = Core::ArchiveIndex::Build(*archive);
    const Core::Extractor extractor(*archive, index);
    Tests::TempDirectory out;
    CHECK(index.GetFolders().size() == 3); // root, sub, sub/dir

    auto stats = extractor.Run(AllEntries(index), out.GetPath());
    CHECK(stats.Extracted == 3);
    CHECK(std::filesystem::exists(out.GetPath() / "sub" / "dir" / "data.bin"));
    CHECK(std::filesystem::exists(out.GetPath() / "empty.dat"));

    std::filesystem::remove(out.GetPath() / "sub" / "dir" / "data.bin");
    int progressCalls = 0;
    Core::ExtractCallbacks callbacks;
    callbacks.OnProgress = [&](std::int64_t done, std::int64_t total, std::string_view name) {
        ++progressCalls;
        CHECK(total == 76800);
        CHECK(name == "sub/dir/data.bin");
        return done < 5000;
    };
    const std::vector<std::uint32_t> big{1};
    stats = extractor.Run(big, out.GetPath(), callbacks);
    CHECK(stats.Cancelled);
    CHECK(stats.Extracted == 0);
    CHECK_FALSE(std::filesystem::exists(out.GetPath() / "sub" / "dir" / "data.bin"));
    CHECK(progressCalls > 0);
}

TEST_CASE("Hostile names cannot escape from the destination")
{
    // names made by hand: the extractor only needs the driver for the bytes, so use an archive and rename through the index
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    const auto archive = driver->Open(Tests::Fixture("test.pak"));
    const auto index = Core::ArchiveIndex::FromNames({"../../evil.txt", "/abs/path.txt", "ok/fine.txt"}, "/");
    Tests::TempDirectory parent;
    const auto out = parent.GetPath() / "out";
    // entries 0..2 of the fake index map to entries 0..2 of test.pak: the bytes do not matter here
    const Core::Extractor extractor(*archive, index);
    const std::vector<std::uint32_t> all{0, 1, 2};
    const auto stats = extractor.Run(all, out);
    CHECK(stats.Failed == 0);
    CHECK(std::filesystem::exists(out / "evil.txt"));
    CHECK(std::filesystem::exists(out / "abs" / "path.txt"));
    CHECK(std::filesystem::exists(out / "ok" / "fine.txt"));
    CHECK_FALSE(std::filesystem::exists(parent.GetPath() / "evil.txt"));
}
