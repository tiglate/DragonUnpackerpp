// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Small helpers on UTF-8 file names, for the formats that live in several files.
#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace Dud
{
// A UTF-8 path as a std::filesystem path, and back: what the C ABI passes is UTF-8, and on Windows path::string() would
// give the ANSI code page instead
inline auto PathOf(const std::string &utf8) -> std::filesystem::path
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

inline auto Utf8Of(const std::filesystem::path &path) -> std::string
{
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

inline auto ToUpper(std::string text) -> std::string
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

inline auto ToLower(std::string text) -> std::string
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// The last part of the path
inline auto FileNameOf(const std::string &path) -> std::string
{
    return path.substr(path.find_last_of("/\\") == std::string::npos ? 0 : path.find_last_of("/\\") + 1);
}

// The extension of the last part of the path, with its dot (empty if none)
inline auto ExtensionOf(const std::string &path) -> std::string
{
    const auto slash = path.find_last_of("/\\");
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return "";
    return path.substr(dot);
}

// The path with another extension (ext starts with a dot, or is empty to remove it)
inline auto ChangeFileExt(const std::string &path, const std::string &ext) -> std::string
{
    return path.substr(0, path.size() - ExtensionOf(path).size()) + ext;
}

inline auto FileExists(const std::string &path) -> bool
{
    std::error_code error;
    return std::filesystem::is_regular_file(std::filesystem::path(std::u8string(path.begin(), path.end())), error);
}

// The file with another extension (in lower or upper case) next to the given one; empty if none
inline auto Sibling(const std::string &fileName, const std::string &ext) -> std::string
{
    std::string result = ChangeFileExt(fileName, ToLower(ext));
    if (!FileExists(result))
        result = ChangeFileExt(fileName, ToUpper(ext));
    if (!FileExists(result))
        result.clear();
    return result;
}
} // namespace Dud
