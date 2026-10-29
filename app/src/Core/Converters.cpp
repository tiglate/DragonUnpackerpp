// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/Converters.h"

#include "Core/DdsDecoder.h"
#include "Core/PngEncoder.h"
#include "Core/TgaDecoder.h"

#include <algorithm>
#include <cctype>

namespace Dup::Core
{
namespace
{
auto ExtensionOf(const std::string &name) -> std::string
{
    const auto dot = name.rfind('.');
    if (dot == std::string::npos)
    {
        return {};
    }
    std::string extension = name.substr(dot + 1);
    for (auto &c : extension)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension;
}
} // namespace

auto IsConvertible(const std::string &fileName) -> bool
{
    const auto extension = ExtensionOf(fileName);
    return extension == "tga" || extension == "dds";
}

auto ConvertForExtraction(const std::string &fileName, const std::string &formatId, std::span<const std::byte> bytes) -> std::optional<ConvertedFile>
{
    const auto extension = ExtensionOf(fileName);
    std::optional<DecodedImage> image;
    if (extension == "tga")
    {
        image = DecodeTga(bytes);
    }
    else if (extension == "dds")
    {
        image = DecodeDds(bytes).Image;
        if (image && formatId.rfind("Unity", 0) == 0)
        {
            FlipRows(*image); // the rows of the DDS files of Unity textures are stored bottom first
        }
    }
    if (!image)
    {
        return std::nullopt;
    }
    auto png = EncodePng(*image);
    if (!png)
    {
        return std::nullopt;
    }
    return ConvertedFile{"png", std::move(*png)};
}
} // namespace Dup::Core
