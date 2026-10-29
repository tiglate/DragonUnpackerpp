// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/RipNaming.h"

#include <cstdio>
#include <set>

namespace Dup::Core
{
namespace
{
auto Replace(std::string text, const std::string &token, const std::string &value) -> std::string
{
    for (std::size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size()))
    {
        text.replace(at, token.size(), value);
    }
    return text;
}

auto Number(const char *format, long long value) -> std::string
{
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, format, value);
    return buffer;
}
} // namespace

auto GetRipKind(const std::string &extension) -> RipKind
{
    static const std::set<std::string> audio{"wav", "mid", "voc", "669", "xm", "s3m", "it", "mp3", "aac", "m4a", "ogg"};
    static const std::set<std::string> video{"avi", "mov", "mp4", "bik", "fli", "flc", "mkv", "webm"};
    static const std::set<std::string> image{"png", "jpg", "gif", "bmp", "dds", "emf", "wmf", "lbm", "tga", "webp"};
    if (audio.contains(extension))
    {
        return RipKind::Audio;
    }
    if (video.contains(extension))
    {
        return RipKind::Video;
    }
    return image.contains(extension) ? RipKind::Image : RipKind::Unknown;
}

auto MakeRipNames(const std::filesystem::path &scanned, const std::vector<RippedFile> &files, const NamingOptions &options) -> std::vector<std::string>
{
    std::string base = options.Template.empty() ? NamingOptions::DEFAULT_RIP_TEMPLATE : options.Template;
    for (char &c : base)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
        {
            c = '_';
        }
    }
    std::string extension = scanned.extension().string();
    if (!extension.empty() && extension[0] == '.')
    {
        extension.erase(0, 1);
    }
    base = Replace(base, "%f", scanned.stem().string());
    base = Replace(base, "%x", extension);

    std::vector<std::string> names;
    names.reserve(files.size());
    for (std::size_t i = 0; i < files.size(); ++i)
    {
        const RippedFile &file = files[i];
        std::string name = Replace(base, "%o", std::to_string(file.Offset));
        name = Replace(name, "%h", Number("%016llX", file.Offset));
        name = Replace(name, "%s", std::to_string(file.Size));
        name = Replace(name, "%l", Number("%016llX", file.Size));
        name = Replace(name, "%n", Number("%010lld", static_cast<long long>(i + 1)));
        name += "." + file.Extension;
        if (options.MakeDirs)
        {
            switch (GetRipKind(file.Extension))
            {
            case RipKind::Audio:
                name = "Audio\\" + name;
                break;
            case RipKind::Video:
                name = "Video\\" + name;
                break;
            case RipKind::Image:
                name = "Image\\" + name;
                break;
            case RipKind::Unknown:
                name = "Unknown\\" + name;
                break;
            }
        }
        names.push_back(std::move(name));
    }
    return names;
}
} // namespace Dup::Core
