// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/DriverHost.h"

#include <spdlog/spdlog.h>

#include <cstdlib>

namespace Dup::Core
{
auto PluginPath::Find(const std::filesystem::path &startDirectory) -> PluginPath
{
    namespace fs = std::filesystem;
    if (const char *fromEnvironment = std::getenv("DUP_PLUGIN"); fromEnvironment != nullptr && *fromEnvironment != '\0')
    {
        return PluginPath{fromEnvironment};
    }
    for (fs::path directory = startDirectory; !directory.empty(); directory = directory.parent_path())
    {
#ifdef _WIN32
        for (const char *relative : {"plugins/drv_default.d6d", "build/plugins/drv_default.d6d"})
#else
        for (const char *relative : {"plugins/libdrv_default.d6d.so", "lib/dragonunpacker/plugins/libdrv_default.d6d.so", "build/plugins/libdrv_default.d6d.so"})
#endif
        {
            std::error_code error;
            if (fs::exists(directory / relative, error))
            {
                return PluginPath{directory / relative};
            }
        }
        if (directory == directory.root_path())
        {
            break;
        }
    }
#ifdef _WIN32
    return PluginPath{"drv_default.d6d"}; // the Free Pascal library is called libdrv_default.d6d.so on Linux and drv_default.d6d on Windows
#else
    return PluginPath{"libdrv_default.d6d.so"};
#endif
}

DriverHost::DriverHost(const PluginPath &plugin) : _path(plugin.Path)
{
    try
    {
        _driver = Dudi7::Driver::Load(_path);
        spdlog::info("driver \"{}\" loaded from {}: {} formats", _driver->GetName(), _path.string(), _driver->GetFormats().size());
    }
    catch (const std::exception &error)
    {
        _error = error.what();
        spdlog::error("cannot load the driver {}: {}", _path.string(), _error);
    }
}
} // namespace Dup::Core
