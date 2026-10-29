// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/AppSettings.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <system_error>
#include <utility>

namespace Dup::Core
{
namespace
{
auto Trim(const std::string &text) -> std::string
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

auto ParseInt(const std::string &text, int fallback, int minimum, int maximum) -> int
{
    try
    {
        return std::clamp(std::stoi(text), minimum, maximum);
    }
    catch (const std::exception &)
    {
        return fallback;
    }
}
} // namespace

auto SettingsFile::Default() -> SettingsFile
{
    std::filesystem::path base;
#ifdef _WIN32
    if (const char *appData = std::getenv("APPDATA"); appData != nullptr && *appData != '\0')
    {
        return SettingsFile{std::filesystem::path(appData) / "dragonunpacker6" / "dragonunpacker6.ini"};
    }
#endif
    if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0')
    {
        base = xdg;
    }
    else if (const char *home = std::getenv("HOME"); home != nullptr && *home != '\0')
    {
        base = std::filesystem::path(home) / ".config";
    }
    else
    {
        base = std::filesystem::temp_directory_path();
    }
    return SettingsFile{base / "dragonunpacker6" / "dragonunpacker6.ini"};
}

AppSettings::AppSettings(SettingsFile file) : _file(std::move(file))
{
}

void AppSettings::Load()
{
    _recent.clear();
    std::ifstream stream(_file.Path);
    if (!stream)
    {
        return;
    }
    std::string section;
    std::string line;
    std::vector<std::pair<int, std::string>> recent;
    while (std::getline(stream, line))
    {
        line = Trim(line);
        if (line.empty() || line.front() == ';' || line.front() == '#')
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }
        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));
        if (section == "Paths" && key == "LastOpenDir")
        {
            LastOpenDirectory = value;
        }
        else if (section == "Paths" && key == "LastExtractDir")
        {
            LastExtractDirectory = value;
        }
        else if (section == "Appearance" && key == "Theme")
        {
            Theme = value == "light" ? "light" : "dark";
        }
        else if (section == "General" && key == "Language")
        {
            Language = value;
        }
        else if (section == "General" && key == "DefaultView")
        {
            DefaultView = value == "thumbnails" ? "thumbnails" : "details";
        }
        else if (section == "General" && key == "ToolsDirectory")
        {
            ToolsDirectory = value;
        }
        else if (section == "General" && key == "TemplatesDirectory")
        {
            TemplatesDirectory = value;
        }
        else if (section == "General" && key == "AddDetectedExtension")
        {
            AddDetectedExtension = value == "1";
        }
        else if (section == "General" && key == "ThumbnailSize")
        {
            ThumbnailSize = ParseInt(value, ThumbnailSize, 48, 256);
        }
        else if (section == "Preview" && key == "ImageMegabytes")
        {
            PreviewImageMegabytes = ParseInt(value, PreviewImageMegabytes, 1, 1024);
        }
        else if (section == "Preview" && key == "TextKilobytes")
        {
            PreviewTextKilobytes = ParseInt(value, PreviewTextKilobytes, 1, 65536);
        }
        else if (section == "Preview" && key == "PalettePath")
        {
            PalettePath = value;
        }
        else if (section == "Log" && key == "Level")
        {
            LogLevel = value;
        }
        else if (section == "Layout")
        {
            Layout[key] = value;
        }
        else if (section == "Recent" && !value.empty())
        {
            try
            {
                recent.emplace_back(std::stoi(key), value);
            }
            catch (const std::exception &)
            {
                // an entry with a bad key is ignored
            }
        }
    }
    std::ranges::sort(recent, {}, &std::pair<int, std::string>::first);
    for (const auto &[number, path] : recent)
    {
        if (_recent.size() < MAX_RECENT)
        {
            _recent.push_back(path);
        }
    }
}

auto AppSettings::Save() const -> bool
{
    std::error_code error;
    std::filesystem::create_directories(_file.Path.parent_path(), error);
    if (error)
    {
        return false;
    }
    std::ofstream stream(_file.Path, std::ios::trunc);
    if (!stream)
    {
        return false;
    }
    stream << "[Paths]\nLastOpenDir=" << LastOpenDirectory << "\nLastExtractDir=" << LastExtractDirectory << "\n\n";
    stream << "[Appearance]\nTheme=" << Theme << "\n\n";
    stream << "[General]\nLanguage=" << Language << "\nDefaultView=" << DefaultView << "\nThumbnailSize=" << ThumbnailSize << "\nAddDetectedExtension=" << (AddDetectedExtension ? 1 : 0) << "\nToolsDirectory=" << ToolsDirectory << "\nTemplatesDirectory=" << TemplatesDirectory << "\n\n";
    stream << "[Preview]\nImageMegabytes=" << PreviewImageMegabytes << "\nTextKilobytes=" << PreviewTextKilobytes << "\nPalettePath=" << PalettePath << "\n\n";
    stream << "[Log]\nLevel=" << LogLevel << "\n\n";
    stream << "[Layout]\n";
    for (const auto &[key, value] : Layout)
    {
        stream << key << '=' << value << '\n';
    }
    stream << "\n[Recent]\n";
    for (std::size_t i = 0; i < _recent.size(); ++i)
    {
        stream << i << '=' << _recent[i] << '\n';
    }
    return stream.good();
}

void AppSettings::AddRecent(const std::string &file)
{
    RemoveRecent(file);
    _recent.insert(_recent.begin(), file);
    if (_recent.size() > MAX_RECENT)
    {
        _recent.resize(MAX_RECENT);
    }
}

void AppSettings::RemoveRecent(const std::string &file)
{
    std::erase(_recent, file);
}

void AppSettings::ClearRecent()
{
    _recent.clear();
}
} // namespace Dup::Core
