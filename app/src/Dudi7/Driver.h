// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Dudi7/IArchive.h"
#include "Dudi7/SharedLibrary.h"
#include "dudi7.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace Dup::Dudi7
{
struct FormatInfo
{
    std::string Extensions; // ex: *.PAK;*.WAD
    std::string Name;
};

class Driver;

/// An open archive. Must not outlive its Driver.
class Archive : public IArchive
{
  public:
    ~Archive() override;

    [[nodiscard]] auto GetEntryCount() const -> std::int64_t override
    {
        return _entryCount;
    }
    [[nodiscard]] auto GetFormatId() const -> const std::string & override
    {
        return _formatId;
    }
    /// "/" or "\\"
    [[nodiscard]] auto GetDirSeparator() const -> const std::string & override
    {
        return _dirSeparator;
    }
    [[nodiscard]] auto GetEntry(std::int64_t index) const -> EntryInfo override;

    /// Sends the entry's bytes to the sink in pieces; the sink returns false to cancel.
    /// Returns the status of the driver (0 = done, DUDI7_ERR_CANCELLED when the sink said no).
    auto Extract(std::int64_t index, const std::function<bool(std::span<const std::byte>)> &sink) const -> int override;

  private:
    friend class Driver;
    Archive(const Driver &driver, void *handle);
    [[nodiscard]] auto LastError() const -> std::string;

    const Driver &_driver;
    void *_handle;
    std::int64_t _entryCount{0};
    std::string _formatId;
    std::string _dirSeparator;
};

/// A loaded driver library (".d6d"). Not copyable; archives opened from it must be closed before it is destroyed.
class Driver
{
  public:
    /// Loads and initializes the library; throws Error/std::runtime_error when it is not a DUDI 7 driver.
    static auto Load(const std::filesystem::path &path) -> std::unique_ptr<Driver>;
    ~Driver();
    Driver(const Driver &) = delete;
    auto operator=(const Driver &) -> Driver & = delete;

    [[nodiscard]] auto GetName() const -> const std::string &
    {
        return _name;
    }
    [[nodiscard]] auto GetFormats() const -> const std::vector<FormatInfo> &
    {
        return _formats;
    }

    /// Throws Error(DUDI7_ERR_NOTFORMAT...) when no format of the driver recognizes the file.
    [[nodiscard]] auto Open(const std::filesystem::path &path) const -> std::unique_ptr<Archive>;

  private:
    friend class Archive;
    explicit Driver(SharedLibrary library);

    struct Functions
    {
        int32_t (*Version)(int32_t, int32_t *){nullptr};
        int32_t (*Init)(const dudi7_host *){nullptr};
        int32_t (*Shutdown)(){nullptr};
        int32_t (*GetDriverInfo)(dudi7_driver_info *){nullptr};
        int32_t (*GetFormatInfo)(int32_t, dudi7_format_info *){nullptr};
        int32_t (*Open)(const char *, int32_t, void **){nullptr};
        int32_t (*GetArchiveInfo)(void *, dudi7_archive_info *){nullptr};
        int32_t (*GetEntry)(void *, int64_t, dudi7_entry *){nullptr};
        int32_t (*Extract)(void *, int64_t, dudi7_write_cb, void *){nullptr};
        int32_t (*Close)(void *){nullptr};
        int32_t (*GetLastError)(void *, char *, int32_t){nullptr};
    };

    SharedLibrary _library;
    Functions _functions;
    std::string _name;
    std::vector<FormatInfo> _formats;
};
} // namespace Dup::Dudi7
