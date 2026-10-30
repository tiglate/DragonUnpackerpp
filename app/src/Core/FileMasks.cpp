// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/FileMasks.h"

#include <algorithm>
#include <cctype>

namespace Dup::Core
{
namespace
{
auto Lower(std::string text) -> std::string
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

void AddOnce(std::vector<std::string> &list, const std::string &mask)
{
    if (!mask.empty() && std::ranges::find(list, mask) == list.end())
    {
        list.push_back(mask);
    }
}
} // namespace

auto ExpandMasks(std::string_view masks) -> std::vector<std::string>
{
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= masks.size())
    {
        std::size_t end = masks.find(';', start);
        if (end == std::string_view::npos)
        {
            end = masks.size();
        }
        const std::string mask(masks.substr(start, end - start));
        AddOnce(result, mask);
        AddOnce(result, Lower(mask));
        start = end + 1;
    }
    return result;
}
} // namespace Dup::Core
