// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/ListTemplates.h"

#include <QCoreApplication>
#include <QFile>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace Dup::Ui
{
namespace
{
auto ReadFile(const std::filesystem::path &path) -> std::optional<std::vector<std::byte>>
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return std::nullopt;
    }
    std::vector<char> raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(raw.size());
    std::memcpy(bytes.data(), raw.data(), raw.size());
    return bytes;
}

auto ReadResource(const QString &path) -> std::optional<std::vector<std::byte>>
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    const QByteArray data = file.readAll();
    std::vector<std::byte> bytes(static_cast<std::size_t>(data.size()));
    std::memcpy(bytes.data(), data.constData(), bytes.size());
    return bytes;
}

auto ToText(const std::vector<std::byte> &bytes) -> std::string
{
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}
} // namespace

auto DefaultTemplatesDirectory(const std::filesystem::path &settingsFile) -> std::filesystem::path
{
    return settingsFile.parent_path() / "templates";
}

auto LoadListTemplates(const std::filesystem::path &userDirectory) -> std::vector<LoadedTemplate>
{
    std::vector<LoadedTemplate> result;
    for (const char *id : {"text-list", "text-db", "xml-db", "homepage"})
    {
        const QString base = QString(":/list_templates/%1/").arg(id);
        const auto source = ReadResource(base + "template.tsf");
        if (!source)
        {
            continue;
        }
        try
        {
            result.push_back({std::string("bundled:") + id, Core::ParseTsf(ToText(*source), [&](const std::string &name) { return ReadResource(base + QString::fromStdString(name)); })});
        }
        catch (const std::exception &error)
        {
            spdlog::warn("template {}: {}", id, error.what());
        }
    }
    std::error_code ignored;
    if (!std::filesystem::is_directory(userDirectory, ignored))
    {
        return result;
    }
    std::vector<std::filesystem::path> files;
    for (const auto &item : std::filesystem::recursive_directory_iterator(userDirectory, ignored))
    {
        std::string extension = item.path().extension().string();
        for (auto &c : extension)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (item.is_regular_file() && (extension == ".uht" || extension == ".tsf" || extension == ".ts"))
        {
            files.push_back(item.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto &path : files)
    {
        try
        {
            const auto bytes = ReadFile(path);
            if (!bytes)
            {
                continue;
            }
            std::string extension = path.extension().string();
            for (auto &c : extension)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (extension == ".uht")
            {
                result.push_back({path.string(), Core::ParseUht(*bytes)});
            }
            else
            {
                const auto folder = path.parent_path();
                result.push_back({path.string(), Core::ParseTsf(ToText(*bytes), [&](const std::string &name) {
                                      std::string portable = name;
                                      std::replace(portable.begin(), portable.end(), '\\', '/');
                                      return ReadFile(folder / portable);
                                  })});
            }
        }
        catch (const std::exception &error)
        {
            spdlog::warn("template {}: {}", path.string(), error.what());
        }
    }
    return result;
}

auto ListText(const std::string &keyword) -> std::string
{
    // the keywords of the language files of v5; the texts are translated by the interface
    struct Text
    {
        const char *Keyword;
        const char *Source;
    };
    static const Text texts[] = {
        {"LSTCP1", QT_TRANSLATE_NOOP("ListTexts", "File")},
        {"LSTCP2", QT_TRANSLATE_NOOP("ListTexts", "Size (Bytes)")},
        {"LSTCP3", QT_TRANSLATE_NOOP("ListTexts", "Offset")},
        {"LSTCP4", QT_TRANSLATE_NOOP("ListTexts", "Description")},
        {"LSTCP5", QT_TRANSLATE_NOOP("ListTexts", "Data X")},
        {"LSTCP6", QT_TRANSLATE_NOOP("ListTexts", "Data Y")},
        {"EX", QT_TRANSLATE_NOOP("ListTexts", "File %e")},
        {"EXANIM", QT_TRANSLATE_NOOP("ListTexts", "Animation (%e)")},
        {"EXART", QT_TRANSLATE_NOOP("ListTexts", "Art Package (Textures)")},
        {"EXBIN", QT_TRANSLATE_NOOP("ListTexts", "Data (%e)")},
        {"EXCFG", QT_TRANSLATE_NOOP("ListTexts", "Configuration (%e)")},
        {"EXDLL", QT_TRANSLATE_NOOP("ListTexts", "Dynamic Link Library")},
        {"EXFIRE", QT_TRANSLATE_NOOP("ListTexts", "Fire Texture (Dynamic)")},
        {"EXIMG", QT_TRANSLATE_NOOP("ListTexts", "Image %e (%d)")},
        {"EXMAP", QT_TRANSLATE_NOOP("ListTexts", "Level Map (%e)")},
        {"EXMDL", QT_TRANSLATE_NOOP("ListTexts", "3D Model (%e)")},
        {"EXMPEG", QT_TRANSLATE_NOOP("ListTexts", "MPEG Audio/Video")},
        {"EXMUS", QT_TRANSLATE_NOOP("ListTexts", "Music (%d)")},
        {"EXPAL", QT_TRANSLATE_NOOP("ListTexts", "Palette")},
        {"EXSCRP", QT_TRANSLATE_NOOP("ListTexts", "Script (%e)")},
        {"EXSND", QT_TRANSLATE_NOOP("ListTexts", "Sound (%d)")},
        {"EXSPR", QT_TRANSLATE_NOOP("ListTexts", "Sprite")},
        {"EXTEX", QT_TRANSLATE_NOOP("ListTexts", "Texture (%e)")},
        {"EXTXT", QT_TRANSLATE_NOOP("ListTexts", "Text Document")},
    };
    for (const auto &text : texts)
    {
        if (keyword == text.Keyword)
        {
            return QCoreApplication::translate("ListTexts", text.Source).toStdString();
        }
    }
    return {};
}
} // namespace Dup::Ui
