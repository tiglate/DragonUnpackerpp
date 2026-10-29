// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Dup::Core
{
/// One find of the HyperRipper as stored in an HRF file.
struct HrfEntry
{
    std::string Name;
    std::int64_t Offset{0};
    std::int64_t Size{0};
};

struct HrfInfo
{
    std::string Title;
    std::string Author;
    std::string Url;
};

/// A HyperRipper File: the offsets, sizes and names of what a scan found in a companion file, so that the finds can be
/// reopened without scanning again (versions 1, 2 and 3.0 of the format of v5; see docs/formats/HRF.txt).
struct HrfFile
{
    int Version{3};                  // 1, 2 or 3
    std::string SourceName;          // the companion file (name only)
    std::int64_t SourceSize{0};      // its size, to check that it is the right one
    int ProgramVersion{60040};       // version of the program that made it (see HrfProgramVersion)
    std::uint8_t ProgramId{1};       // 1 = HyperRipper, 0 = anonymous (version 3)
    std::optional<HrfInfo> Info;     // versions 2 and 3
    std::vector<HrfEntry> Entries;
};

/// The version written in the files: 6.0 in the numbering of the format.
constexpr int HRF_PROGRAM_VERSION = 60040;
constexpr std::uint8_t HRF_PROGRAM_ID = 1;

/// Writes the file; names longer than the format allows (version 1: 31 characters, 2: 63, 3: 254) are cut and version 1
/// and 2 cannot hold offsets or sizes beyond 2 GiB (std::runtime_error). The members of the entries are used as they are.
void WriteHrf(const std::string &path, const HrfFile &hrf);

/// Reads versions 1, 2 and 3.0; std::runtime_error with a message if it is not an HRF file or is damaged.
[[nodiscard]] auto ReadHrf(const std::string &path) -> HrfFile;
} // namespace Dup::Core
