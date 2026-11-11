// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/AppSettings.h"
#include "TestSupport.h"

using namespace Dup::Core;

TEST_CASE("AppSettings keeps the recent files, the directories and the layout")
{
    Dup::Tests::TempDirectory directory;
    const SettingsFile file{directory.GetPath() / "sub" / "a.ini"};

    AppSettings settings(file);
    settings.Load();
    CHECK(settings.GetRecent().empty());
    for (int i = 1; i <= 14; ++i)
    {
        settings.AddRecent("/tmp/file" + std::to_string(i));
    }
    settings.AddRecent("/tmp/file12");
    settings.LastOpenDirectory = "/tmp/a b";
    settings.LastExtractDirectory = "/tmp/out";
    settings.Theme = "light";
    settings.Layout["geometry"] = "AAEC/w==";
    settings.Language = "pt_BR";
    settings.DefaultView = "thumbnails";
    settings.ThumbnailSize = 128;
    settings.PreviewImageMegabytes = 16;
    settings.PreviewTextKilobytes = 512;
    settings.LogLevel = "warn";
    CHECK(settings.GetRecent().size() == AppSettings::MAX_RECENT);
    CHECK(settings.GetRecent()[0] == "/tmp/file12");
    CHECK(settings.GetRecent()[1] == "/tmp/file14");
    REQUIRE(settings.Save());

    AppSettings reloaded(file);
    reloaded.Load();
    CHECK(reloaded.GetRecent() == settings.GetRecent());
    CHECK(reloaded.LastOpenDirectory == "/tmp/a b");
    CHECK(reloaded.LastExtractDirectory == "/tmp/out");
    CHECK(reloaded.Theme == "light");
    CHECK(reloaded.Layout["geometry"] == "AAEC/w==");
    CHECK(reloaded.Language == "pt_BR");
    CHECK(reloaded.DefaultView == "thumbnails");
    CHECK(reloaded.ThumbnailSize == 128);
    CHECK(reloaded.PreviewImageMegabytes == 16);
    CHECK(reloaded.PreviewTextKilobytes == 512);
    CHECK(reloaded.LogLevel == "warn");

    reloaded.RemoveRecent("/tmp/file12");
    REQUIRE(reloaded.Save());
    AppSettings again(file);
    again.Load();
    CHECK(again.GetRecent().size() == AppSettings::MAX_RECENT - 1);
    CHECK(again.GetRecent()[0] == "/tmp/file14");
}

TEST_CASE("AppSettings ignores a damaged file")
{
    Dup::Tests::TempDirectory directory;
    const auto path = directory.GetPath() / "bad.ini";
    std::ofstream(path) << "garbage\n[Recent]\nx=/a\n3=/b\n[Appearance]\nTheme=pink\n=\n[General]\nThumbnailSize=99999\n[Preview]\nImageMegabytes=abc\n";
    AppSettings settings(SettingsFile{path});
    settings.Load();
    CHECK(settings.GetRecent() == std::vector<std::string>{"/b"});
    CHECK(settings.Theme == "system");
    CHECK(settings.ThumbnailSize == 256); // out of range values are clamped, unreadable ones ignored
    CHECK(settings.PreviewImageMegabytes == 64);
}
