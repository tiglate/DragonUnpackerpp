// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/EntryListExport.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ostream>

namespace Dup::Core
{
namespace
{
auto CsvField(std::string_view text) -> std::string
{
    if (text.find_first_of(",\"\r\n") == std::string_view::npos)
    {
        return std::string(text);
    }
    std::string quoted = "\"";
    for (const char c : text)
    {
        if (c == '"')
        {
            quoted += '"';
        }
        quoted += c;
    }
    return quoted + "\"";
}

auto JsonString(std::string_view text) -> std::string
{
    std::string result = "\"";
    char buffer[8];
    for (const char c : text)
    {
        switch (c)
        {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                result += buffer;
            }
            else
            {
                result += c; // UTF-8 bytes are kept as they are
            }
        }
    }
    return result + "\"";
}
} // namespace

void WriteEntryList(const ArchiveIndex &index, std::span<const std::uint32_t> entries, ListFormat format, std::ostream &out)
{
    if (format == ListFormat::CSV)
    {
        out << "name,size,offset\n";
    }
    else if (format == ListFormat::JSON)
    {
        out << "[";
    }
    bool first = true;
    for (const auto entry : entries)
    {
        const auto path = index.GetPath(entry);
        switch (format)
        {
        case ListFormat::CSV:
            out << CsvField(path) << ',' << index.GetSize(entry) << ',' << index.GetOffset(entry) << '\n';
            break;
        case ListFormat::JSON:
            out << (first ? "\n  " : ",\n  ") << "{\"name\": " << JsonString(path) << ", \"size\": " << index.GetSize(entry)
                << ", \"offset\": " << index.GetOffset(entry) << '}';
            break;
        case ListFormat::TEXT:
            out << path << '\n';
            break;
        }
        first = false;
    }
    if (format == ListFormat::JSON)
    {
        out << (entries.empty() ? "]\n" : "\n]\n");
    }
}

auto ListFormatForFile(const std::string &fileName) -> ListFormat
{
    const auto dot = fileName.rfind('.');
    std::string extension = dot == std::string::npos ? std::string() : fileName.substr(dot + 1);
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == "csv" ? ListFormat::CSV : extension == "json" ? ListFormat::JSON : ListFormat::TEXT;
}
} // namespace Dup::Core
