// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Dup::Core
{
enum class FileCategory
{
    UNKNOWN,
    IMAGE,
    AUDIO,
    VIDEO,
    ARCHIVE, // containers and compressed data
    MODEL,
    FONT,
    TEXT,
    DOCUMENT,
    CODE, // programs, libraries, scripts and bytecode
    DATA  // other known binary formats
};

/// What a file is, found from its content.
struct DetectedType
{
    std::string Id;          // "png", "wad2"...: short and stable
    std::string Description; // "PNG image"
    std::string Extension;   // the usual extension without the dot ("png"), empty when there is none
    FileCategory Category{FileCategory::UNKNOWN};
    /// 0 to 100. 100: a signature that cannot be a coincidence and a structure that holds; about 60: a signature of two or three
    /// bytes or a weak structure; 30 or less: only guessed (text, a name that looks right).
    int Confidence{0};
};

/// How many bytes at the start of the file the detector looks at ("head"): enough for every signature it knows except
/// the ISO 9660 one that is further.
constexpr std::size_t FILE_TYPE_HEAD_BYTES = 4096;

/// Finds the type of a file from its first bytes. `size` is the size of the whole file (some formats are checked against
/// it), `fileName` is only used when the content does not tell (an extension that is known gives a guess of low confidence)
/// and may be empty. Nothing at all is returned for data that looks like nothing known, except text and binary data.
[[nodiscard]] auto IdentifyFile(std::span<const std::byte> head, std::int64_t size, std::string_view fileName = {}) -> DetectedType;

/// The name of a category in English ("Image", "Audio"...); the interface translates it.
[[nodiscard]] auto GetCategoryName(FileCategory category) -> const char *;

/// Every type that has a signature, for the tests and the documentation.
[[nodiscard]] auto GetKnownTypes() -> std::vector<DetectedType>;

/// The extension to give to an entry that has none, or nullopt if the type is not sure enough (confidence below 60) or has no
/// extension.
[[nodiscard]] auto SuggestExtension(const DetectedType &type) -> std::optional<std::string>;
} // namespace Dup::Core
