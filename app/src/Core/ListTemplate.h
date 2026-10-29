// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Dup::Core
{
/// The templates of the lists of entries ("DUHT", Dragon UnPACKer HTML Templates, of v5): a header, a text repeated for each entry
/// (the variable) and a footer, with macros between $$ ($$FILE$$, $$SIZE$$, $$FSIZE$$, $$HOFFSET$$, $$DIRECTORY$$,
/// $$FILENAME$$, $$FORMAT$$, $$DESC$$, $$TOTNUMFILES$$...; see docs/formats/DUHT.txt), images that go
/// next to the list, and a preview picture. Made from a source (the TSF files of DUHTComp) or read from a compiled .uht file.
struct ListTemplateInfo
{
    std::string Name;
    std::string Version;
    std::string Author;
    std::string Email;
    std::string Url;
    std::string DefaultExtension; // of the list: "TXT", "HTML"...
    std::string ExtensionInfo;    // what the list is: "Dragon UnPACKer Text Database (*.TXT)"
};

struct TemplateImage
{
    std::string Name;
    std::vector<std::byte> Data;
};

struct ListTemplate
{
    ListTemplateInfo Info;
    std::string Header;
    std::string Footer;
    std::string Variable;
    std::vector<std::byte> Preview; // a JPEG picture
    std::vector<TemplateImage> Images;
    bool Compress{true}; // when it is compiled (Compression of the source)
};

/// Reads a compiled template (DUHT version 2); std::runtime_error when it is not one or is damaged (CRC32 of every part checked).
[[nodiscard]] auto ParseUht(std::span<const std::byte> bytes) -> ListTemplate;

/// Writes a compiled template as DUHTComp did (version 2, revision 1); the parts are compressed (zlib) when that makes them smaller.
[[nodiscard]] auto CompileUht(const ListTemplate &list, bool compress = true) -> std::vector<std::byte>;

/// Reads a file that the source of a template names (relative to it); nullopt if it does not exist.
using TemplateFileReader = std::function<std::optional<std::vector<std::byte>>(const std::string &name)>;

/// Reads the source of a template (a TSF file: keywords between {TSF} and {/TSF}); std::runtime_error with the line of the
/// error, as DUHTComp reported it. The files it names (HeaderFile, FooterFile, VariableFile, Preview, IncludeFile) are read through
/// the callback.
[[nodiscard]] auto ParseTsf(const std::string &text, const TemplateFileReader &reader) -> ListTemplate;

struct ListEntry
{
    std::string Name; // with its folders, separated as the archive does
    std::int64_t Offset{0};
    std::int64_t Size{0};
    std::int64_t DataX{0};
    std::int64_t DataY{0};
};

enum class ListSort
{
    NONE,
    NAME,
    SIZE,
    OFFSET
};

void SortEntries(std::vector<ListEntry> &entries, ListSort sort, bool invert);

/// What the macros of a template stand for besides the entries.
struct ListContext
{
    std::string ArchiveName;        // $$FILENAME$$: name of the file that is open
    std::string Format;             // $$FORMAT$$: identifier of the format
    std::string Directory;          // $$DIRECTORY$$ of the header and the footer: the folder of the list (with its separator), or ""
    std::string Separator;          // between the folders of the names of the entries
    std::string AppVersion{"6.0.0"}; // $$DUPVER$$
    std::string AppEdition;          // $$DUPEDIT$$
    std::string AppUrl;              // $$DUPURL$$
    std::string ThousandsSeparator{","}; // $$FSIZE$$...
    /// Text for the keywords of $$LNG<KEYWORD>$$ (the language keywords of v5); empty if unknown.
    std::function<std::string(const std::string &)> Language;
    /// Description of an entry from its extension ($$DESC$$).
    std::function<std::string(const std::string &)> Describe;
};

/// The list: header, the variable once per entry, footer.
[[nodiscard]] auto RenderList(const ListTemplate &list, const ListContext &context, const std::vector<ListEntry> &entries) -> std::string;

/// Descriptions of the types of files by extension, as v5 gave them ("Sound (RIFF/WAVE)", "Image BMP (Windows BitMaP)"...).
/// The text callback gives the texts by keyword: "EXANIM" ("Animation (%e)"), "EXIMG" ("Image %e (%d)")...; without one the English
/// texts of v5 are used.
[[nodiscard]] auto DescribeExtension(const std::string &extension, const std::function<std::string(const std::string &)> &text = {}) -> std::string;

/// The English texts of v5 for the keywords of DescribeExtension.
[[nodiscard]] auto GetDefaultDescriptionText(const std::string &keyword) -> std::string;
} // namespace Dup::Core
