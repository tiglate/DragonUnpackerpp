// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Registry.h"

#include <algorithm>
#include <cctype>

#include "FormatList.h"

namespace Dud
{
namespace
{
auto Mutable() -> std::vector<FormatDef> &
{
    static std::vector<FormatDef> formats;
    return formats;
}

auto Upper(std::string text) -> std::string
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}
} // namespace

void AddFormat(FormatDef format)
{
    Mutable().push_back(std::move(format));
}

auto Formats() -> const std::vector<FormatDef> &
{
    // Thread safe (C++11 static initialisation); the formats are only read afterwards
    static const bool registered = []() {
#define DUD_FORMAT(Name) Register##Name();
#include "FormatList.inc"
#undef DUD_FORMAT
        return true;
    }();
    (void)registered;
    return Mutable();
}

auto ExtensionMatches(const std::string &masks, const std::string &fileName) -> bool
{
    const auto slash = fileName.find_last_of("/\\");
    const auto dot = fileName.find_last_of('.');
    const std::string ext =
        "*" + Upper(dot == std::string::npos || (slash != std::string::npos && dot < slash) ? "" : fileName.substr(dot));
    const std::string list = Upper(masks);
    size_t start = 0;
    while (start <= list.size())
    {
        size_t end = list.find(';', start);
        if (end == std::string::npos)
            end = list.size();
        if (list.compare(start, end - start, ext) == 0)
            return true;
        start = end + 1;
    }
    return false;
}
} // namespace Dud
