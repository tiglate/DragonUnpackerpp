// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Dup::Core
{
/// A command line program that converts files, described by an INI file as in the "cnv_exttools" plugin of v5:
///
///   [cnv_exttools]
///   enabled=1
///   name=ImageMagick (PNG)
///   author=, url=, comment=
///   path=magick           (a program on the PATH, a full path or a path relative to the folder of the INI files)
///   command="%i" "%o"     (the arguments: %i is the input file, %o the output file; no shell is used)
///   resultext=png         (extension of the output)
///   resultoktest=0        (how the exit code is compared with resultok: 0 equal, 1 greater, 2 less, 3 at least, 4 at most, 5 different)
///   resultok=0
///   extensions=bmp tga    (the files it can convert, by extension, separated by spaces)
struct ExternalTool
{
    std::string IniName;
    bool Enabled{false};
    std::string Name;
    std::string Author;
    std::string Url;
    std::string Comment;
    std::string Path;    // as written
    std::string Command;
    std::string ResultExtension;
    int ResultTest{0};
    int ResultOk{0};
    std::vector<std::string> Extensions; // lower case, without the dot
    std::filesystem::path Program;       // where Path was found (empty: not found, the tool is not usable)

    /// Enabled, the program exists and the command has %i and %o.
    [[nodiscard]] auto IsUsable() const -> bool;
    [[nodiscard]] auto Handles(const std::string &extension) const -> bool;
};

/// The tools described by the *.ini files of a folder (the files that have no [cnv_exttools] section are ignored).
[[nodiscard]] auto LoadExternalTools(const std::filesystem::path &directory) -> std::vector<ExternalTool>;

/// Where the INI files are by default: next to the settings, in "tools".
[[nodiscard]] auto DefaultToolsDirectory(const std::filesystem::path &settingsFile) -> std::filesystem::path;

/// The arguments of a command line: split at the spaces that are not inside double quotes (the quotes are removed).
[[nodiscard]] auto SplitCommand(const std::string &command) -> std::vector<std::string>;

/// Does the exit code of the program mean success for this tool (resultoktest)?
[[nodiscard]] auto IsResultOk(const ExternalTool &tool, int exitCode) -> bool;

/// Runs the tool on the bytes (written to a temporary file with the given extension) and returns what it wrote, or
/// nullopt if the program could not be started, the exit code is not a success and nothing was written, or the output is empty.
[[nodiscard]] auto RunExternalTool(const ExternalTool &tool, std::span<const std::byte> input, const std::string &inputExtension) -> std::optional<std::vector<std::byte>>;
} // namespace Dup::Core
