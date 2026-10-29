// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ArchiveIndex.h"
#include "Core/ExternalTools.h"
#include "Core/TextureConverters.h"
#include "Dudi7/Driver.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Dup::Core
{
enum class ConflictAction
{
    OVERWRITE,
    OVERWRITE_ALL,
    SKIP,
    SKIP_ALL,
    CANCEL
};

struct ExtractStats
{
    int Extracted{0};
    int Skipped{0};
    int Failed{0};
    bool Cancelled{false};
    std::vector<std::string> Errors; // one message per entry that could not be extracted
};

/// What the caller decides during an extraction. Either callback may be empty (conflicts then overwrite).
struct ExtractCallbacks
{
    /// The file exists already.
    std::function<ConflictAction(const std::filesystem::path &)> OnConflict;
    /// Bytes done / total of the whole job, and the name of the current entry; return false to cancel.
    std::function<bool(std::int64_t, std::int64_t, std::string_view)> OnProgress;
};

/// What the extraction does besides copying.
struct ExtractOptions
{
    bool ConvertImages{false}; // TGA and DDS images, and the textures of TextureConverters.h, are written as PNG (see Converters.h)
    /// Names that have no extension get the one that the content says (FileTypes.h) when it is sure enough: the entries of the archives
    /// that keep only hashes or numbers as names become files that other programs open.
    bool AddDetectedExtension{false};
    /// A command line tool (ExternalTools.h) that converts the entries it handles by their extension; the other entries are copied.
    std::shared_ptr<const ExternalTool> Tool;
    /// The palette of the 8 bits pictures that have none (Quake, Build tiles) when the archive has no PALETTE.DAT, PALETTE.LMP...
    std::optional<PaletteRgb> FallbackPalette;
};

/// Extracts entries of an open archive to a directory. No widget and no thread here: the caller decides where it
/// runs (the callbacks are called from the calling thread). Every name goes through EntryNames.
class Extractor
{
  public:
    Extractor(const Dudi7::IArchive &archive, const ArchiveIndex &index) : _archive(archive), _index(index)
    {
    }

    /// Extracts the entries (numbers in the index) to destinationDirectory.
    [[nodiscard]] auto Run(std::span<const std::uint32_t> entries, const std::filesystem::path &destinationDirectory,
                           const ExtractCallbacks &callbacks = {}, const ExtractOptions &options = {}) const -> ExtractStats;

  private:
    const Dudi7::IArchive &_archive;
    const ArchiveIndex &_index;
};
} // namespace Dup::Core
