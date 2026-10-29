// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The formats known to the driver, in the order their signatures are tried.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Archive.h"

namespace Dud
{
struct FormatDef
{
    std::string Extensions; // ex: *.PAK;*.WAD
    std::string Name;       // ex: Quake (*.PAK)
    bool (*Check)(Reader &);
    bool RequiresExtension;
    std::unique_ptr<Archive> (*Create)();
};

// All the formats, built on first use
auto Formats() -> const std::vector<FormatDef> &;

// For the Register<Name> functions of the format files
void AddFormat(FormatDef format);

template <typename T> void Register(std::string extensions, std::string name)
{
    AddFormat({std::move(extensions), std::move(name), &T::Check, T::RequiresExtension(),
               []() -> std::unique_ptr<Archive> { return std::make_unique<T>(); }});
}

// Is the extension of fileName one of the ";" separated masks (ex: *.PAK;*.WAD)?
auto ExtensionMatches(const std::string &masks, const std::string &fileName) -> bool;
} // namespace Dud
