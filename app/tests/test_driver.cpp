// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/DriverHost.h"
#include "Dudi7/Driver.h"
#include "TestSupport.h"

#include <atomic>
#include <cstdio>
#include <thread>

using namespace Dup;

TEST_CASE("The default driver loads and lists its formats")
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    CHECK(driver->GetName() == "Default Driver");
    CHECK(driver->GetFormats().size() > 80);
    CHECK(driver->GetFormats().front().Extensions == "*.PAK");
}

TEST_CASE("A file that is not a DUDI 7 driver is refused")
{
    CHECK_THROWS_AS(Dudi7::Driver::Load("/nonexistent/lib.so"), std::runtime_error);
    CHECK_THROWS_AS(Dudi7::Driver::Load("/lib/x86_64-linux-gnu/libm.so.6"), Dudi7::Error);
}

TEST_CASE("DriverHost keeps a load failure instead of throwing")
{
    const Core::DriverHost host(Core::PluginPath{"/nonexistent/lib.so"});
    CHECK_FALSE(host.IsLoaded());
    CHECK_FALSE(host.GetError().empty());
}

TEST_CASE("Entries, extraction, cancel and limits through the C ABI")
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    const auto archive = driver->Open(Tests::Fixture("test.pak"));
    CHECK(archive->GetFormatId() == "PACK");
    REQUIRE(archive->GetEntryCount() == 3);

    const auto entry = archive->GetEntry(1);
    CHECK(entry.Name == "sub/dir/data.bin");
    CHECK(entry.Size == 76800);

    const auto bytes = archive->ExtractToMemory(0, 1024);
    CHECK(std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size()) == "Hello, Dragon UnPACKer!\n");

    CHECK_THROWS_AS(archive->ExtractToMemory(1, 1000), Dudi7::Error); // larger than the limit

    std::size_t seen = 0;
    const auto status = archive->Extract(1, [&](std::span<const std::byte> piece) {
        seen += piece.size();
        return seen < 5000;
    });
    CHECK(status == DUDI7_ERR_CANCELLED);
    CHECK(seen >= 5000);
    CHECK(seen < 76800);
}

TEST_CASE("Opening something that is not an archive fails with a message")
{
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    Tests::TempDirectory directory;
    const auto path = directory.GetPath() / "junk.bin";
    std::ofstream(path, std::ios::binary) << "this is not an archive at all, just some text";
    try
    {
        const auto archive = driver->Open(path);
        FAIL("the junk file was accepted");
    }
    catch (const Dudi7::Error &error)
    {
        CHECK(error.GetStatus() == DUDI7_ERR_NOTFORMAT);
        CHECK(std::string(error.what()).size() > 0);
    }
}

TEST_CASE("Independent archives can be used from several threads at the same time")
{
    // The extraction jobs of the interface run on worker threads with their own archive handle. The driver is written in
    // Pascal and the threads are created here (not by the Pascal runtime), including the exception paths of the driver.
    const auto driver = Dudi7::Driver::Load(Tests::PluginPath());
    Tests::TempDirectory directory;
    const auto junk = directory.GetPath() / "junk.bin";
    std::ofstream(junk, std::ios::binary) << "this is not an archive at all, just some text";
    const std::vector<std::filesystem::path> files{Tests::Fixture("test.pak"), Tests::Fixture("dups.wad"), Tests::Fixture("obj22.bundle"),
                                                   Tests::Fixture("lzx.xnb")};
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t)
    {
        threads.emplace_back([&, t] {
            for (int round = 0; round < 60; ++round)
            {
                try
                {
                    const auto &file = files[static_cast<std::size_t>(t + round) % files.size()];
                    const auto archive = driver->Open(file);
                    for (std::int64_t i = 0; i < archive->GetEntryCount(); ++i)
                    {
                        const auto info = archive->GetEntry(i);
                        std::int64_t total = 0;
                        const auto status = archive->Extract(i, [&](std::span<const std::byte> bytes) {
                            total += static_cast<std::int64_t>(bytes.size());
                            return true;
                        });
                        if (status != DUDI7_OK || total != info.Size)
                        {
                            std::fprintf(stderr, "entry %lld of %s: status %d size %lld expected %lld\n", static_cast<long long>(i), file.c_str(), status,
                                         static_cast<long long>(total), static_cast<long long>(info.Size));
                            ++failures;
                        }
                    }
                    try
                    {
                        const auto bad = driver->Open(junk); // raises (and catches) an exception inside the driver
                        std::fprintf(stderr, "junk was accepted\n");
                        ++failures;
                    }
                    catch (const Dudi7::Error &)
                    {
                    }
                }
                catch (const std::exception &error)
                {
                    std::fprintf(stderr, "exception: %s\n", error.what());
                    ++failures;
                }
            }
        });
    }
    for (auto &thread : threads)
    {
        thread.join();
    }
    CHECK(failures == 0);
}
