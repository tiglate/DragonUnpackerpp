// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "dudi7.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace Dup::Dudi7
{
/// A failed call of a driver: the status code of the contract and the driver's text.
class Error : public std::runtime_error
{
  public:
    Error(int status, const std::string &message) : std::runtime_error(message), _status(status)
    {
    }
    [[nodiscard]] auto GetStatus() const -> int
    {
        return _status;
    }

  private:
    int _status;
};

struct EntryInfo
{
    std::string Name; // UTF-8, with the separator of the archive
    std::int64_t Offset{0};
    std::int64_t Size{0};
    std::int64_t DataX{0}; // what the driver keeps for the entry (the width of an ART tile, the type of a lump...)
    std::int64_t DataY{0};
};

/// What the rest of the application needs from an archive: the entries of a driver's archive, or of a virtual one (the files
/// found by the HyperRipper). One object is used by one thread at a time.
class IArchive
{
  public:
    IArchive() = default;
    virtual ~IArchive() = default;
    IArchive(const IArchive &) = delete;
    auto operator=(const IArchive &) -> IArchive & = delete;

    [[nodiscard]] virtual auto GetEntryCount() const -> std::int64_t = 0;
    [[nodiscard]] virtual auto GetFormatId() const -> const std::string & = 0;
    /// "/" or "\\" ("" for flat archives)
    [[nodiscard]] virtual auto GetDirSeparator() const -> const std::string & = 0;
    [[nodiscard]] virtual auto GetEntry(std::int64_t index) const -> EntryInfo = 0;
    /// Sends the entry's bytes to the sink in pieces; the sink returns false to cancel. Returns a DUDI7_* status
    /// (0 = done, DUDI7_ERR_CANCELLED when the sink said no).
    virtual auto Extract(std::int64_t index, const std::function<bool(std::span<const std::byte>)> &sink) const -> int = 0;

    /// The whole entry in memory; throws Error(DUDI7_ERR_BUFFER) when it is larger than maxBytes.
    [[nodiscard]] auto ExtractToMemory(std::int64_t index, std::size_t maxBytes) const -> std::vector<std::byte>;
};
} // namespace Dup::Dudi7
