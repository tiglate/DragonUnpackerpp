// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Dup::Core
{
/// Where the settings are kept (a distinct type so that the injector can bind it).
struct SettingsFile
{
    std::filesystem::path Path;

    /// $XDG_CONFIG_HOME/dragonunpacker6/dragonunpacker6.ini (~/.config/... by default).
    [[nodiscard]] static auto Default() -> SettingsFile;
};

/// User settings kept between two runs: recent files, last directories, theme, window layout (opaque text values written
/// by the interface). Stored in an INI file; no widget and no Qt here.
class AppSettings
{
  public:
    static constexpr std::size_t MAX_RECENT = 10;

    explicit AppSettings(SettingsFile file);

    void Load();
    /// Writes the file again from scratch (no stale entries); a failure is reported by the return value.
    auto Save() const -> bool;

    /// Most recent first, at most MAX_RECENT, no duplicates.
    void AddRecent(const std::string &file);
    void RemoveRecent(const std::string &file);
    void ClearRecent();
    [[nodiscard]] auto GetRecent() const -> const std::vector<std::string> &
    {
        return _recent;
    }

    std::string LastOpenDirectory;
    std::string LastExtractDirectory;
    std::string Theme{"system"}; // "system" (as the desktop), "dark" or "light"
    std::string Language;      // "" = the language of the system, else "en" or "pt_BR"...
    std::string DefaultView{"details"}; // "details" or "thumbnails"
    int ThumbnailSize{96};
    int PreviewImageMegabytes{64}; // the largest image the preview decodes
    int PreviewTextKilobytes{1024};
    std::string ToolsDirectory; // the INI files of the external conversion tools (ExternalTools.h); empty = the folder "tools" next to the settings
    std::string TemplatesDirectory; // the templates of the lists of entries of the user (ListTemplate.h); empty = the folder "templates" next to the settings
    bool AddDetectedExtension{false}; // when extracting, give the entries without an extension the one that their content says
    std::string PalettePath; // the palette of the 8 bits pictures that have none (Quake, Build): a 768 bytes or JASC .pal file
    std::string LogLevel{"debug"}; // trace, debug, info, warn, error
    /// Free text values of the interface (window geometry, dock layout...), written in the [Layout] section.
    std::map<std::string, std::string> Layout;

    [[nodiscard]] auto GetFile() const -> const std::filesystem::path &
    {
        return _file.Path;
    }

  private:
    SettingsFile _file;
    std::vector<std::string> _recent;
};
} // namespace Dup::Core
