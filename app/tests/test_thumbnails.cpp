// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ArchiveIndex.h"
#include "Core/ThumbnailWorker.h"
#include "TestSupport.h"

#include <condition_variable>

using namespace Dup::Core;

TEST_CASE("Downscale keeps small images and averages big ones")
{
    DecodedImage small{2, 2, std::vector<std::uint8_t>(16, 9)};
    CHECK(Downscale(small, 4).Rgba == small.Rgba);

    // 4 x 4 image: left half white, right half black, opaque
    DecodedImage big;
    big.Width = big.Height = 4;
    big.Rgba.resize(64);
    for (std::size_t i = 0; i < 16; ++i)
    {
        const std::uint8_t v = (i % 4) < 2 ? 255 : 0;
        big.Rgba[i * 4] = big.Rgba[i * 4 + 1] = big.Rgba[i * 4 + 2] = v;
        big.Rgba[i * 4 + 3] = 255;
    }
    const auto half = Downscale(big, 2);
    REQUIRE(half.Width == 2);
    REQUIRE(half.Height == 2);
    CHECK(half.Rgba[0] == 255);
    CHECK(half.Rgba[4] == 0);
    CHECK(half.Rgba[3] == 255);

    // transparent pixels do not darken the colour of the others
    DecodedImage edge;
    edge.Width = 2;
    edge.Height = 1;
    edge.Rgba = {200, 100, 50, 255, 0, 0, 0, 0};
    const auto one = Downscale(edge, 1);
    CHECK(one.Rgba[0] == 200);
    CHECK(one.Rgba[1] == 100);
    CHECK(one.Rgba[3] == 127);
}

TEST_CASE("HasThumbnail by extension")
{
    CHECK(HasThumbnail("a.tga"));
    CHECK(HasThumbnail("A.DDS"));
    CHECK(HasThumbnail("x.png"));
    CHECK_FALSE(HasThumbnail("x.txt"));
    CHECK_FALSE(HasThumbnail("noextension"));
}

TEST_CASE("The thumbnail worker serves requests from its own handle")
{
    const auto driver = Dup::Dudi7::Driver::Load(Dup::Tests::PluginPath());
    const auto file = Dup::Tests::Fixture("obj22.bundle");
    const auto archive = driver->Open(file);
    auto index = std::make_shared<const ArchiveIndex>(ArchiveIndex::Build(*archive));
    std::uint32_t wanted = 0;
    for (std::uint32_t i = 0; i < index->GetEntryCount(); ++i)
    {
        if (index->GetFileName(i) == "rgba.tga")
        {
            wanted = i;
        }
    }
    REQUIRE(wanted != 0);

    std::mutex mutex;
    std::condition_variable done;
    std::vector<ThumbnailResult> results;
    {
        ThumbnailWorker worker(std::make_shared<DriverArchiveSource>(*driver, file), index, archive->GetFormatId(), 64, [&](ThumbnailResult result) {
            const std::scoped_lock lock(mutex);
            results.push_back(std::move(result));
            done.notify_all();
        });
        worker.Request(wanted);
        std::unique_lock lock(mutex);
        REQUIRE(done.wait_for(lock, std::chrono::seconds(10), [&] { return !results.empty(); }));
    }
    CHECK(results[0].Entry == wanted);
    CHECK(results[0].Image.Width == 2);
    CHECK(results[0].Image.Height == 2);
}
