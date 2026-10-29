// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/HrfFile.h"
#include "Core/RipNaming.h"
#include "TestSupport.h"

#include <fstream>

using namespace Dup::Core;

namespace
{
auto Sample(int version) -> HrfFile
{
    HrfFile hrf;
    hrf.Version = version;
    hrf.SourceName = "game.dat";
    hrf.SourceSize = 123456;
    if (version >= 2)
    {
        hrf.Info = HrfInfo{"A title", "An author", "http://example.org/"};
    }
    hrf.Entries = {{"one.png", 0, 100}, {"sub\\two.wav", 4096, 2000}, {"three.mp3", 100000, 5}};
    return hrf;
}
} // namespace

TEST_CASE("HRF files of the three versions are read back as written")
{
    Dup::Tests::TempDirectory directory;
    for (int version = 1; version <= 3; ++version)
    {
        INFO("version " << version);
        const auto path = (directory.GetPath() / ("v" + std::to_string(version) + ".hrf")).string();
        WriteHrf(path, Sample(version));
        const HrfFile back = ReadHrf(path);
        CHECK(back.Version == version);
        CHECK(back.SourceName == "game.dat");
        CHECK(back.SourceSize == 123456);
        REQUIRE(back.Entries.size() == 3);
        CHECK(back.Entries[1].Name == "sub\\two.wav");
        CHECK(back.Entries[1].Offset == 4096);
        CHECK(back.Entries[2].Size == 5);
        CHECK(back.Entries[2].Offset == 100000);
        CHECK(back.Info.has_value() == (version >= 2));
        if (version >= 2)
        {
            CHECK(back.Info->Title == "A title");
            CHECK(back.Info->Author == "An author");
            CHECK(back.Info->Url == "http://example.org/");
        }
    }
}

TEST_CASE("HRF sizes of the files are those of the specification")
{
    Dup::Tests::TempDirectory directory;
    auto size = [&](int version, bool info) {
        HrfFile hrf = Sample(version);
        if (!info)
        {
            hrf.Info.reset();
        }
        const auto path = (directory.GetPath() / "s.hrf").string();
        WriteHrf(path, hrf);
        return std::filesystem::file_size(path);
    };
    CHECK(size(1, false) == 114 + 3 * 41);
    CHECK(size(2, false) == 114 + 2 + 3 * 89);
    CHECK(size(2, true) == 114 + 2 + 256 + 3 * 89);
    CHECK(size(3, false) == 284 + 3 * 271);
    CHECK(size(3, true) == 284 + 257 + 3 * 271);
}

TEST_CASE("HRF version 3 holds big offsets, the older ones refuse them; damaged files are refused")
{
    Dup::Tests::TempDirectory directory;
    HrfFile big = Sample(3);
    big.Entries.push_back({"huge.bin", 5LL << 30, 1LL << 31});
    const auto path = (directory.GetPath() / "big.hrf").string();
    WriteHrf(path, big);
    CHECK(ReadHrf(path).Entries.back().Offset == (5LL << 30));
    big.Version = 2;
    CHECK_THROWS(WriteHrf(path, big));

    WriteHrf(path, Sample(3));
    std::filesystem::resize_file(path, 500); // cut in the index
    CHECK_THROWS(ReadHrf(path));
    {
        std::ofstream out(path, std::ios::binary);
        out << "not an hrf file at all";
    }
    CHECK_THROWS(ReadHrf(path));
}

TEST_CASE("The names of the finds follow the template of v5")
{
    const std::vector<RippedFile> files{{255, 1000, "PNG", "png"}, {70000, 20, "MP3", "mp3"}, {5, 6, "ZIP", "zip"}};
    const std::filesystem::path scanned = "/games/Data/level.pak";

    auto names = MakeRipNames(scanned, files, NamingOptions{});
    CHECK(names[0] == "level_pak-0000000001.png");
    CHECK(names[1] == "level_pak-0000000002.mp3");

    NamingOptions custom;
    custom.Template = "%f/%o-%h-%s-%l";
    names = MakeRipNames(scanned, files, custom);
    CHECK(names[0] == "level_255-00000000000000FF-1000-00000000000003E8.png"); // the '/' of the template is not a folder
    custom.Template = "";
    CHECK(MakeRipNames(scanned, files, custom)[0] == "level_pak-0000000001.png");

    NamingOptions dirs;
    dirs.MakeDirs = true;
    names = MakeRipNames(scanned, files, dirs);
    CHECK(names[0] == "Image\\level_pak-0000000001.png");
    CHECK(names[1] == "Audio\\level_pak-0000000002.mp3");
    CHECK(names[2] == "Unknown\\level_pak-0000000003.zip");
    CHECK(GetRipKind("avi") == RipKind::Video);
}
