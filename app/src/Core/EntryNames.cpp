// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/EntryNames.h"

#include <algorithm>
#include <cctype>

namespace Dup::Core
{
namespace
{
constexpr std::size_t MAX_COMPONENT_BYTES = 200;
constexpr char PATH_DELIMITER = static_cast<char>(std::filesystem::path::preferred_separator);

auto IsDigit(char c) -> bool
{
    return c >= '0' && c <= '9';
}

auto Lower(std::string text) -> std::string
{
    for (auto &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

/// Cut to maxBytes without splitting a UTF-8 character, keeping the extension.
auto Shorten(const std::string &text, std::size_t maxBytes) -> std::string
{
    if (text.size() <= maxBytes)
    {
        return text;
    }
    const auto dot = text.rfind('.');
    std::string extension;
    if (dot != std::string::npos && dot > 0 && text.size() - dot < 16)
    {
        extension = text.substr(dot);
    }
    std::size_t keep = maxBytes - extension.size();
    while (keep > 0 && (static_cast<unsigned char>(text[keep]) & 0xC0) == 0x80)
    {
        --keep;
    }
    return text.substr(0, keep) + extension;
}
} // namespace

auto SplitEntryName(std::string_view name, std::string_view dirSeparator) -> std::vector<std::string>
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        bool isSeparator = false;
        if (dirSeparator == "/")
        {
            isSeparator = name[i] == '/';
        }
        else if (dirSeparator == "\\")
        {
            isSeparator = name[i] == '\\' || name[i] == '/';
        }
        if (isSeparator)
        {
            parts.emplace_back(name.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.emplace_back(name.substr(start));
    return parts;
}

auto SafeComponent(std::string_view component) -> std::string
{
    std::string result(component);
    for (auto &c : result)
    {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < ' ' || byte == 127 || c == '/')
        {
            c = '_';
        }
#ifdef _WIN32
        else if (std::string_view("\\<>:\"|?*").find(c) != std::string_view::npos)
        {
            c = '_';
        }
#endif
    }
#ifdef _WIN32
    // Windows silently drops trailing dots and spaces
    while (!result.empty() && (result.back() == ' ' || result.back() == '.'))
    {
        result.pop_back();
    }
#endif
    if (result == "." || result == "..")
    {
        result.clear();
    }
    return Shorten(result, MAX_COMPONENT_BYTES);
}

auto SafeRelativePath(std::string_view name, std::string_view dirSeparator, std::int64_t entryIndex) -> std::string
{
    const auto parts = SplitEntryName(name, dirSeparator);
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        std::string component = SafeComponent(parts[i]);
        // the drive letters ("C:") of Windows absolute paths
        if (i + 1 < parts.size() && parts[i].size() == 2 && parts[i][1] == ':')
        {
            component.clear();
        }
        if (component.empty())
        {
            continue;
        }
        if (!result.empty())
        {
            result += PATH_DELIMITER;
        }
        result += component;
    }
    if (result.empty())
    {
        result = "entry_" + std::to_string(entryIndex);
    }
    return result;
}

auto NaturalCompare(std::string_view a, std::string_view b) -> int
{
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() && j < b.size())
    {
        if (IsDigit(a[i]) && IsDigit(b[j]))
        {
            // two numbers: without the leading zeros the longest is the largest
            std::size_t endA = i;
            while (endA < a.size() && IsDigit(a[endA]))
            {
                ++endA;
            }
            std::size_t endB = j;
            while (endB < b.size() && IsDigit(b[endB]))
            {
                ++endB;
            }
            auto numberA = a.substr(i, endA - i);
            auto numberB = b.substr(j, endB - j);
            while (numberA.size() > 1 && numberA.front() == '0')
            {
                numberA.remove_prefix(1);
            }
            while (numberB.size() > 1 && numberB.front() == '0')
            {
                numberB.remove_prefix(1);
            }
            if (numberA.size() != numberB.size())
            {
                return static_cast<int>(numberA.size()) - static_cast<int>(numberB.size());
            }
            if (const int result = numberA.compare(numberB); result != 0)
            {
                return result;
            }
            i = endA;
            j = endB;
        }
        else
        {
            const int ca = std::toupper(static_cast<unsigned char>(a[i]));
            const int cb = std::toupper(static_cast<unsigned char>(b[j]));
            if (ca != cb)
            {
                return ca - cb;
            }
            ++i;
            ++j;
        }
    }
    return static_cast<int>(a.size() - i) - static_cast<int>(b.size() - j);
}

auto IsInsideDir(const std::filesystem::path &root, const std::filesystem::path &path) -> bool
{
    std::string normalizedRoot = std::filesystem::absolute(root).lexically_normal().string();
    if (normalizedRoot.empty() || normalizedRoot.back() != PATH_DELIMITER)
    {
        normalizedRoot += PATH_DELIMITER;
    }
    const std::string normalizedPath = std::filesystem::absolute(path).lexically_normal().string();
    return normalizedPath.size() > normalizedRoot.size() && normalizedPath.compare(0, normalizedRoot.size(), normalizedRoot) == 0;
}

auto NameAllocator::Allocate(const std::string &relativePath) -> std::string
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= relativePath.size())
    {
        const auto end = relativePath.find(PATH_DELIMITER, start);
        if (end == std::string::npos)
        {
            parts.push_back(relativePath.substr(start));
            break;
        }
        parts.push_back(relativePath.substr(start, end - start));
        start = end + 1;
    }

    // directories: one that has the name of an existing file gets a suffix
    std::string prefix;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i)
    {
        std::string candidate = parts[i];
        for (int n = 2; _files.contains(Lower(prefix + candidate)); ++n)
        {
            candidate = parts[i] + "~" + std::to_string(n);
        }
        parts[i] = candidate;
        prefix += candidate + PATH_DELIMITER;
        _directories.insert(Lower(prefix.substr(0, prefix.size() - 1)));
    }

    // the file: unique, and not the name of a directory
    const std::string &last = parts.back();
    const auto dot = last.rfind('.');
    std::string base = last;
    std::string extension;
    if (dot != std::string::npos && dot > 0)
    {
        base = last.substr(0, dot);
        extension = last.substr(dot);
    }
    std::string candidate = last;
    for (int n = 2; _files.contains(Lower(prefix + candidate)) || _directories.contains(Lower(prefix + candidate)); ++n)
    {
        candidate = base + "~" + std::to_string(n) + extension;
    }
    _files.insert(Lower(prefix + candidate));
    return prefix + candidate;
}
} // namespace Dup::Core
