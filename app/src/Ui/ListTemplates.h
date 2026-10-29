// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ListTemplate.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Dup::Ui
{
struct LoadedTemplate
{
    std::string Source; // where it comes from: "bundled:text-list" or the path
    Core::ListTemplate Template;
};

/// The templates of the lists: the four of v5 that are in the executable (text list, text database, XML database, HTML home page),
/// then the ones of the folder of the user: compiled files (*.uht) and the folders that hold a source (*.tsf or *.ts).
/// What cannot be read is skipped (with a message in the log).
[[nodiscard]] auto LoadListTemplates(const std::filesystem::path &userDirectory) -> std::vector<LoadedTemplate>;

/// Where the templates of the user are by default: the folder "templates" next to the settings.
[[nodiscard]] auto DefaultTemplatesDirectory(const std::filesystem::path &settingsFile) -> std::filesystem::path;

/// The texts of $$LNG<KEYWORD>$$ (the column names of v5) and of the descriptions of the types, in the language of the interface.
[[nodiscard]] auto ListText(const std::string &keyword) -> std::string;
} // namespace Dup::Ui
