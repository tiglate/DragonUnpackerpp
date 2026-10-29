// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/Extractor.h"

#include "Core/Converters.h"
#include "Core/EntryNames.h"
#include "Core/PngEncoder.h"
#include "Core/TypeIdentifier.h"

#include <fstream>
#include <system_error>

namespace Dup::Core
{
namespace
{
constexpr std::int64_t CONVERT_LIMIT = 256LL * 1024 * 1024; // larger images are copied as they are
} // namespace

auto Extractor::Run(std::span<const std::uint32_t> entries, const std::filesystem::path &destinationDirectory,
                    const ExtractCallbacks &callbacks, const ExtractOptions &options) const -> ExtractStats
{
    namespace fs = std::filesystem;
    ExtractStats stats;
    std::int64_t total = 0;
    for (const auto entry : entries)
    {
        total += _index.GetSize(entry);
    }
    std::int64_t done = 0;
    bool overwriteAll = false;
    bool skipAll = false;
    const fs::path root = fs::absolute(destinationDirectory);
    NameAllocator allocator;
    std::optional<std::optional<PaletteRgb>> archivePalette; // looked for once, when a picture needs it

    for (const auto entry : entries)
    {
        const std::string fullName(_index.GetPath(entry));
        std::string wanted = SafeRelativePath(fullName, _index.GetDirSeparator(), entry);
        // converted entries: read in memory first (images are small next to the limit) and name the file by its new type
        std::optional<ConvertedFile> converted;
        const std::string wantedExtension = [&] {
            const auto dot = wanted.rfind('.');
            return dot == std::string::npos ? std::string() : wanted.substr(dot + 1);
        }();
        if (options.Tool != nullptr && options.Tool->Handles(wantedExtension) && _index.GetSize(entry) <= CONVERT_LIMIT)
        {
            try
            {
                const auto data = _archive.ExtractToMemory(entry, static_cast<std::size_t>(CONVERT_LIMIT));
                if (auto output = RunExternalTool(*options.Tool, data, wantedExtension))
                {
                    converted = ConvertedFile{options.Tool->ResultExtension, std::move(*output)};
                }
                else
                {
                    stats.Errors.push_back(fullName + ": " + options.Tool->Name + " gave nothing, extracted as it is");
                }
            }
            catch (const std::exception &)
            {
                converted.reset();
            }
            if (converted)
            {
                const auto dot = wanted.rfind('.');
                wanted = wanted.substr(0, dot == std::string::npos ? wanted.size() : dot) + "." + converted->Extension;
            }
        }
        else if (options.ConvertImages && _index.GetSize(entry) <= CONVERT_LIMIT)
        {
            try
            {
                const auto info = _archive.GetEntry(entry);
                const TextureEntry texture{_archive.GetFormatId(), wanted, info.DataX, info.DataY};
                if (IsTextureEntry(texture))
                {
                    const auto data = _archive.ExtractToMemory(entry, static_cast<std::size_t>(CONVERT_LIMIT));
                    const PaletteRgb *palette = nullptr;
                    if (NeedsPalette(texture))
                    {
                        if (!archivePalette)
                        {
                            archivePalette = FindPaletteInArchive(_archive);
                        }
                        palette = archivePalette->has_value() ? &**archivePalette : (options.FallbackPalette ? &*options.FallbackPalette : nullptr);
                    }
                    if (auto image = DecodeTexture(texture, data, palette))
                    {
                        if (auto png = EncodePng(*image))
                        {
                            converted = ConvertedFile{"png", std::move(*png)};
                        }
                    }
                }
                else if (IsConvertible(wanted))
                {
                    const auto data = _archive.ExtractToMemory(entry, static_cast<std::size_t>(CONVERT_LIMIT));
                    converted = ConvertForExtraction(wanted, _archive.GetFormatId(), data);
                }
            }
            catch (const std::exception &)
            {
                converted.reset(); // extracted as it is: the normal path reports a real failure
            }
            if (converted)
            {
                const auto dot = wanted.rfind('.');
                wanted = wanted.substr(0, dot == std::string::npos ? wanted.size() : dot) + "." + converted->Extension;
            }
        }
        if (options.AddDetectedExtension && !converted)
        {
            const auto separator = wanted.find_last_of("/\\");
            const auto dot = wanted.rfind('.');
            if (dot == std::string::npos || (separator != std::string::npos && dot < separator) || dot + 1 == wanted.size())
            {
                try
                {
                    const auto info = _archive.GetEntry(entry);
                    const auto type = IdentifyEntry(_archive, entry, wanted, info.Size);
                    if (const auto extension = SuggestExtension(type))
                    {
                        wanted += (dot + 1 == wanted.size() ? "" : ".") + *extension;
                    }
                }
                catch (const std::exception &)
                {
                    // the entry is extracted with its own name: the normal path reports a real failure
                }
            }
        }
        const auto relative = allocator.Allocate(wanted);
        const fs::path target = root / relative;
        if (!IsInsideDir(root, target))
        {
            ++stats.Failed;
            stats.Errors.push_back(fullName + ": refused (the path is outside of the destination)");
            continue;
        }

        std::error_code ignored;
        if (fs::exists(target, ignored))
        {
            ConflictAction action = ConflictAction::OVERWRITE;
            if (skipAll)
            {
                action = ConflictAction::SKIP;
            }
            else if (!overwriteAll && callbacks.OnConflict)
            {
                action = callbacks.OnConflict(target);
            }
            if (action == ConflictAction::OVERWRITE_ALL)
            {
                overwriteAll = true;
            }
            else if (action == ConflictAction::SKIP_ALL)
            {
                skipAll = true;
            }
            else if (action == ConflictAction::CANCEL)
            {
                stats.Cancelled = true;
                break;
            }
            if (action == ConflictAction::SKIP || action == ConflictAction::SKIP_ALL)
            {
                ++stats.Skipped;
                done += _index.GetSize(entry);
                continue;
            }
        }

        bool ok = false;
        bool cancelled = false;
        std::string failure;
        {
            std::error_code error;
            fs::create_directories(target.parent_path(), error);
            std::ofstream file;
            if (error)
            {
                failure = "cannot create the directory " + target.parent_path().string() + ": " + error.message();
            }
            else
            {
                file.open(target, std::ios::binary | std::ios::trunc);
                if (!file)
                {
                    failure = "cannot create the file " + target.string();
                }
            }
            if (failure.empty() && converted)
            {
                file.write(reinterpret_cast<const char *>(converted->Data.data()), static_cast<std::streamsize>(converted->Data.size()));
                file.flush();
                done += _index.GetSize(entry);
                ok = file.good();
                if (!ok)
                {
                    failure = "write error on " + target.string();
                }
                else if (callbacks.OnProgress && !callbacks.OnProgress(done, total, fullName))
                {
                    cancelled = true; // the file is complete; the next entry is not started
                }
            }
            else if (failure.empty())
            {
                try
                {
                    const auto status = _archive.Extract(entry, [&](std::span<const std::byte> bytes) {
                        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                        if (!file)
                        {
                            failure = "write error on " + target.string();
                            return false;
                        }
                        done += static_cast<std::int64_t>(bytes.size());
                        if (callbacks.OnProgress && !callbacks.OnProgress(done, total, fullName))
                        {
                            cancelled = true;
                            return false;
                        }
                        return true;
                    });
                    if (status == DUDI7_OK)
                    {
                        file.flush();
                        ok = file.good();
                        if (!ok && failure.empty())
                        {
                            failure = "write error on " + target.string();
                        }
                    }
                    else if (!cancelled && failure.empty())
                    {
                        failure = "the driver failed (status " + std::to_string(status) + ")";
                    }
                }
                catch (const std::exception &exception)
                {
                    failure = exception.what();
                }
            }
        }
        if (!ok)
        {
            // never leave a truncated file behind
            std::error_code error;
            fs::remove(target, error);
        }
        if (ok)
        {
            ++stats.Extracted;
            if (cancelled)
            {
                stats.Cancelled = true;
                break;
            }
        }
        else if (cancelled)
        {
            stats.Cancelled = true;
            break;
        }
        else
        {
            ++stats.Failed;
            stats.Errors.push_back(fullName + ": " + failure);
        }
    }
    return stats;
}
} // namespace Dup::Core
