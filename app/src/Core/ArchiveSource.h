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
/// Where an archive comes from, able to open a new handle on it: every thread that reads entries opens its own (one handle
/// per thread). Implementations must be callable from several threads.
class ArchiveSource
{
  public:
    ArchiveSource() = default;
    virtual ~ArchiveSource() = default;
    ArchiveSource(const ArchiveSource &) = delete;
    auto operator=(const ArchiveSource &) -> ArchiveSource & = delete;

    [[nodiscard]] virtual auto Open() const -> std::unique_ptr<Dudi7::IArchive> = 0;
    /// Opens another file with the same driver (the pictures a model uses are in another archive of the game); nullptr when this
    /// kind of source cannot, an exception when the file is not an archive.
    [[nodiscard]] virtual auto OpenFile(const std::filesystem::path & /*path*/) const -> std::unique_ptr<Dudi7::IArchive>
    {
        return nullptr;
    }
    /// A short name for the lists of jobs and the title of the window.
    [[nodiscard]] virtual auto GetTitle() const -> std::string = 0;
    /// The file (for the recent list and the settings), empty for sources that are not a file.
    [[nodiscard]] virtual auto GetPath() const -> std::filesystem::path = 0;
};

/// An archive of a driver; the driver must outlive the source and everything opened from it.
class DriverArchiveSource : public ArchiveSource
{
  public:
    DriverArchiveSource(const Dudi7::Driver &driver, std::filesystem::path path) : _driver(driver), _path(std::move(path))
    {
    }
    [[nodiscard]] auto Open() const -> std::unique_ptr<Dudi7::IArchive> override
    {
        return _driver.Open(_path);
    }
    [[nodiscard]] auto OpenFile(const std::filesystem::path &path) const -> std::unique_ptr<Dudi7::IArchive> override
    {
        return _driver.Open(path);
    }
    [[nodiscard]] auto GetTitle() const -> std::string override
    {
        return _path.filename().string();
    }
    [[nodiscard]] auto GetPath() const -> std::filesystem::path override
    {
        return _path;
    }

  private:
    const Dudi7::Driver &_driver;
    std::filesystem::path _path;
};
} // namespace Dup::Core
