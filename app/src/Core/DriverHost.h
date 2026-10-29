// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Dudi7/Driver.h"

#include <filesystem>
#include <memory>
#include <string>

namespace Dup::Core
{
/// Where the driver library is (a distinct type so that the injector can bind it).
struct PluginPath
{
    std::filesystem::path Path;

    /// Looks for the default driver: $DUP_PLUGIN, then "plugins/libdrv_default.d6d.so" or the build tree,
    /// in the directory startDirectory and in its parents.
    [[nodiscard]] static auto Find(const std::filesystem::path &startDirectory) -> PluginPath;
};

/// The loaded driver for the whole application. A failure to load it is kept (not thrown) so that the window can show it.
class DriverHost
{
  public:
    explicit DriverHost(const PluginPath &plugin);

    [[nodiscard]] auto IsLoaded() const -> bool
    {
        return _driver != nullptr;
    }
    [[nodiscard]] auto GetError() const -> const std::string &
    {
        return _error;
    }
    [[nodiscard]] auto GetPath() const -> const std::filesystem::path &
    {
        return _path;
    }
    /// Only valid when IsLoaded().
    [[nodiscard]] auto GetDriver() const -> const Dudi7::Driver &
    {
        return *_driver;
    }

  private:
    std::filesystem::path _path;
    std::unique_ptr<Dudi7::Driver> _driver;
    std::string _error;
};
} // namespace Dup::Core
