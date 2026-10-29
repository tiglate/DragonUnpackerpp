// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ObjParser.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>

namespace Dup::Core
{
namespace
{
auto NextLine(std::string_view &text) -> std::string_view
{
    const auto end = text.find('\n');
    const auto line = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
    return line;
}

void SkipSpaces(std::string_view &line)
{
    while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r'))
    {
        line.remove_prefix(1);
    }
}

auto Token(std::string_view &line) -> std::string_view
{
    SkipSpaces(line);
    std::size_t end = 0;
    while (end < line.size() && line[end] != ' ' && line[end] != '\t' && line[end] != '\r')
    {
        ++end;
    }
    const auto token = line.substr(0, end);
    line.remove_prefix(end);
    return token;
}

/// Locale independent (strtof would read "0.5" as 0 under a locale with a decimal comma).
auto ParseFloat(std::string_view token, float &value) -> bool
{
    if (!token.empty() && token.front() == '+')
    {
        token.remove_prefix(1);
    }
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    return result.ec == std::errc() && std::isfinite(value);
}
} // namespace

auto ParseObj(std::string_view text, std::size_t maxTriangles) -> std::optional<Mesh>
{
    Mesh mesh;
    while (!text.empty())
    {
        auto line = NextLine(text);
        SkipSpaces(line);
        if (line.size() < 2)
        {
            continue;
        }
        if (line[0] == 'v' && (line[1] == ' ' || line[1] == '\t'))
        {
            line.remove_prefix(1);
            float xyz[3];
            for (float &component : xyz)
            {
                if (!ParseFloat(Token(line), component))
                {
                    return std::nullopt;
                }
            }
            mesh.Positions.insert(mesh.Positions.end(), xyz, xyz + 3);
        }
        else if (line[0] == 'f' && (line[1] == ' ' || line[1] == '\t'))
        {
            line.remove_prefix(1);
            std::vector<std::uint32_t> face;
            const auto vertexCount = static_cast<long>(mesh.Positions.size() / 3);
            for (auto token = Token(line); !token.empty(); token = Token(line))
            {
                long index = 0;
                const auto slash = token.find('/');
                const auto number = token.substr(0, slash);
                if (std::from_chars(number.data(), number.data() + number.size(), index).ec != std::errc())
                {
                    return std::nullopt;
                }
                if (index < 0)
                {
                    index = vertexCount + index + 1; // relative to the end
                }
                if (index < 1 || index > vertexCount)
                {
                    return std::nullopt;
                }
                face.push_back(static_cast<std::uint32_t>(index - 1));
            }
            if (face.size() < 3)
            {
                return std::nullopt;
            }
            for (std::size_t i = 1; i + 1 < face.size(); ++i)
            {
                if (mesh.Triangles.size() / 3 >= maxTriangles)
                {
                    return std::nullopt;
                }
                mesh.Triangles.insert(mesh.Triangles.end(), {face[0], face[i], face[i + 1]});
            }
        }
    }
    if (mesh.Positions.empty() || mesh.Triangles.empty())
    {
        return std::nullopt;
    }
    for (int axis = 0; axis < 3; ++axis)
    {
        mesh.Min[axis] = mesh.Max[axis] = mesh.Positions[static_cast<std::size_t>(axis)];
    }
    for (std::size_t v = 0; v < mesh.Positions.size(); v += 3)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            mesh.Min[axis] = std::min(mesh.Min[axis], mesh.Positions[v + static_cast<std::size_t>(axis)]);
            mesh.Max[axis] = std::max(mesh.Max[axis], mesh.Positions[v + static_cast<std::size_t>(axis)]);
        }
    }
    return mesh;
}
} // namespace Dup::Core
